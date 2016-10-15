/* 
/ spatialite_gml
/
/ a tool loading GML into a SpatiaLite DB
/
/ version 1.0, 2010 August 5
/
/ Author: Sandro Furieri a.furieri@lqt.it
/
/ Copyright (C) 2010  Alessandro Furieri
/
/    This program is free software: you can redistribute it and/or modify
/    it under the terms of the GNU General Public License as published by
/    the Free Software Foundation, either version 3 of the License, or
/    (at your option) any later version.
/
/    This program is distributed in the hope that it will be useful,
/    but WITHOUT ANY WARRANTY; without even the implied warranty of
/    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/    GNU General Public License for more details.
/
/    You should have received a copy of the GNU General Public License
/    along with this program.  If not, see <http://www.gnu.org/licenses/>.
/
*/

#if defined(_WIN32) && !defined(__MINGW32__)
/* MSVC strictly requires this include [off_t] */
#include <sys/types.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <limits.h>

#include <expat.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#include "config-msvc.h"
#else
#include "config.h"
#endif

#ifdef SPATIALITE_AMALGAMATION
#include <spatialite/sqlite3.h>
#else
#include <sqlite3.h>
#endif

#include <spatialite/gaiageo.h>
#include <spatialite.h>

#define ARG_NONE		0
#define ARG_GML_PATH	1
#define ARG_DB_PATH		2
#define ARG_TABLE		3

#define BUFFSIZE	8192

#define COLUMN_NONE	0
#define COLUMN_INT	1
#define COLUMN_FLOAT	2
#define COLUMN_TEXT	3

#define GEOM_NONE	0
#define GEOM_POINT	1
#define GEOM_LINESTRING	2
#define GEOM_POLYGON	3
#define GEOM_MULTIPOINT	4
#define GEOM_MULTILINESTRING	5
#define GEOM_MULTIPOLYGON	6
#define GEOM_COLLECTION	7

#define VALUE_NULL	0
#define VALUE_INT	1
#define VALUE_FLOAT	2
#define VALUE_TEXT	3

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp	_stricmp
#endif /* not WIN32 */

#if defined(_WIN32)
#define atol_64		_atoi64
#else
#define atol_64		atoll
#endif

struct gml_column
{
/* an auxiliary struct used for Colums */
    char *name;
    int type;
    char *txt_value;
    sqlite3_int64 int_value;
    double dbl_value;
    int value_type;
    struct gml_column *next;
};

struct gml_rings_list
{
/* an auxiliary struct used for Polygon's Interior Rings */
    gaiaDynamicLinePtr ring;
    struct gml_rings_list *next;
};

struct gml_rings_collection
{
/* an auxiliary struct used for Polygons */
    gaiaDynamicLinePtr exterior;
    struct gml_rings_list *first;
    struct gml_rings_list *last;
};

struct gml_params
{
/* an auxiliary struct used for GML parsing */
    sqlite3 *db_handle;
    sqlite3_stmt *stmt;
    int geometry_type;
    int declared_type;
    int srid;
    char fid_tag[1024];
    char fid_prefix[1024];
    char fid_name[1024];
    char *CharData;
    int CharDataLen;
    int CharDataMax;
    int CharDataStep;
    int is_feature;
    int is_fid;
    int is_point;
    int is_multi_point;
    int is_linestring;
    int is_multi_linestring;
    int is_polygon;
    int is_multi_polygon;
    int is_outer_boundary;
    int is_inner_boundary;
    int geometry_column;
    char coord_decimal;
    char coord_cs;
    char coord_ts;
    struct gml_column *first;
    struct gml_column *last;
    gaiaGeomCollPtr geometry;
    struct gml_rings_collection polygon;
};

static void
split_fid (const char *org, char *prefix, char *name)
{
/* extracting the <FID> prefix */
    char dummy[1024];
    int i;

    strcpy (dummy, org);
    for (i = 0; i < (int) strlen (dummy); i++)
      {
	  if (dummy[i] == ':')
	    {
		dummy[i] = '\0';
		strcpy (prefix, dummy);
		strcpy (name, dummy + i + 1);
		return;
	    }
      }

    *prefix = '\0';
    strcpy (name, org);
}

static void
check_start_fid (struct gml_params *params, const char *el, const char **attr)
{
/* checking if this element contains a "FID" attribute */
    int count = 0;
    const char *k;
    const char **attrib = attr;
    char prefix[1024];
    char name[1024];

    while (*attrib != NULL)
      {
	  if ((count % 2) == 0)
	      k = *attrib;
	  else
	    {
		if (strcasecmp (k, "fid") == 0)
		  {
		      split_fid (el, prefix, name);
		      params->is_fid = 1;
		      strcpy (params->fid_tag, el);
		      strcpy (params->fid_prefix, prefix);
		      strcpy (params->fid_name, name);
		  }
		if (strcasecmp (k, "gml:id") == 0)
		  {
		      split_fid (el, prefix, name);
		      params->is_fid = 1;
		      strcpy (params->fid_tag, el);
		      strcpy (params->fid_prefix, prefix);
		      strcpy (params->fid_name, name);
		  }
	    }
	  attrib++;
	  count++;
      }
}

static void
check_end1_fid (struct gml_params *params, const char *el)
{
/* checking if the FID tag ends here */
    if (strcasecmp (params->fid_tag, el) == 0)
	params->is_fid = 0;
}

static int
get_data_type (const char *data)
{
/* guessing data type */
    int digits = 0;
    int points = 0;
    int signs = 0;
    int others = 0;
    int i;
    int len = strlen (data);
    for (i = 0; i < len; i++)
      {
	  if (data[i] >= '0' && data[i] <= '9')
	      digits++;
	  else if (data[i] == '.')
	      points++;
	  else if (data[i] == '+' || data[i] == '-')
	      signs++;
	  else
	      others++;
      }
    if (others)
	return COLUMN_TEXT;
    if (digits > 0 && points == 0 && signs <= 1)
	return COLUMN_INT;
    if (digits > 0 && points == 1 && signs <= 1)
	return COLUMN_FLOAT;
    if (len == 0)
	return COLUMN_NONE;
    return COLUMN_TEXT;
}

static void
add_column (struct gml_params *params, const char *name, int type)
{
/* adding/checking a Column */
    int len;
    struct gml_column *col = params->first;
    while (col)
      {
	  if (strcasecmp (col->name, name) == 0)
	    {
		if (col->type == type)
		    return;
		if (col->type == COLUMN_NONE)
		  {
		      /* setting a data type */
		      col->type = type;
		      return;
		  }
		if (type == COLUMN_TEXT)
		  {
		      /* promoting a Column to be TEXT */
		      col->type = COLUMN_TEXT;
		      return;
		  }
		if (col->type == COLUMN_INT && type == COLUMN_FLOAT)
		  {
		      /* promoting a Column to be FLOAT */
		      col->type = COLUMN_FLOAT;
		      return;
		  }
		return;
	    }
	  col = col->next;
      }

    col = malloc (sizeof (struct gml_column));
    len = strlen (name);
    col->name = malloc (len + 1);
    strcpy (col->name, name);
    col->type = type;
    col->txt_value = NULL;
    col->next = NULL;
    if (params->first == NULL)
	params->first = col;
    if (params->last != NULL)
	params->last->next = col;
    params->last = col;
}

static void
column_name (struct gml_params *params, const char *el)
{
/* handling a column name */
    char prefix[1024];
    char name[1024];
    int type;
    if (params->is_point || params->is_multi_point || params->is_linestring
	|| params->is_multi_linestring || params->is_polygon
	|| params->is_multi_polygon)
	return;
    if (params->geometry_column)
	return;
    else
	type = get_data_type (params->CharData);
    split_fid (el, prefix, name);
    if (strcasecmp (prefix, params->fid_prefix) != 0)
	return;
    add_column (params, name, type);
}

static void
set_geometry (struct gml_params *params, int type)
{
/* setting the Geometry Type */
    if (params->geometry_type == GEOM_NONE)
      {
	  /* all right, this is the first time */
	  params->geometry_type = type;
	  return;
      }
    if (params->geometry_type == type)
	return;
    if (params->geometry_type == GEOM_MULTIPOINT && type == GEOM_POINT)
	return;
    if (params->geometry_type == GEOM_MULTILINESTRING
	&& type == GEOM_LINESTRING)
	return;
    if (params->geometry_type == GEOM_MULTIPOLYGON && type == GEOM_POLYGON)
	return;
    if (params->geometry_type == GEOM_POINT && type == GEOM_MULTIPOINT)
      {
	  params->geometry_type = GEOM_MULTIPOINT;
	  return;
      }
    if (params->geometry_type == GEOM_LINESTRING
	&& type == GEOM_MULTILINESTRING)
      {
	  params->geometry_type = GEOM_MULTILINESTRING;
	  return;
      }
    if (params->geometry_type == GEOM_POLYGON && type == GEOM_MULTIPOLYGON)
      {
	  params->geometry_type = GEOM_MULTIPOLYGON;
	  return;
      }
    params->geometry_type = GEOM_COLLECTION;
}

static int
parse_srid (const char *srs)
{
/* trying to parse the SRID from SRS name */
    int i;
    int srid = -1;
    for (i = strlen (srs) - 1; i >= 0; i--)
      {
	  if (srs[i] == '#' || srs[i] == ':')
	    {
		srid = atoi (srs + i + 1);
		return srid;
	    }
      }
    return srid;
}

static void
set_srid (struct gml_params *params, const char **attr)
{
/* setting the SRID */
    int count = 0;
    const char *k;
    const char *v;
    const char **attrib = attr;

    while (*attrib != NULL)
      {
	  if ((count % 2) == 0)
	      k = *attrib;
	  else
	    {
		v = *attrib;
		if (strcasecmp (k, "srsName") == 0)
		  {
		      int srid = parse_srid (v);
		      if (params->srid == INT_MIN)
			{
			    /* this one is the first time */
			    params->srid = srid;
			    return;
			}
		      if (srid == params->srid)
			  return;
		      params->srid = -1;
		      return;
		  }
	    }
	  attrib++;
	  count++;
      }
}

static void
gmlCharData (void *data, const XML_Char * s, int len)
{
/* parsing XML char data */
    struct gml_params *params = (struct gml_params *) data;
    if ((params->CharDataLen + len) > params->CharDataMax)
      {
	  /* we must increase the CharData buffer size */
	  void *new_buf;
	  int new_size = params->CharDataMax;
	  while (new_size < params->CharDataLen + len)
	      new_size += params->CharDataStep;
	  new_buf = realloc (params->CharData, new_size);
	  if (new_buf)
	    {
		params->CharData = new_buf;
		params->CharDataMax = new_size;
	    }
      }
    memcpy (params->CharData + params->CharDataLen, s, len);
    params->CharDataLen += len;
}

static void
start1_tag (void *data, const char *el, const char **attr)
{
/* GML element starting [Pass I] */
    struct gml_params *params = (struct gml_params *) data;
    if (params->is_point || params->is_multi_point || params->is_linestring
	|| params->is_multi_linestring || params->is_polygon
	|| params->is_multi_polygon)
	return;
    params->geometry_column = 0;
    if (strcasecmp (el, "gml:Point") == 0)
      {
	  set_srid (params, attr);
	  params->is_point = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiPoint") == 0)
      {
	  set_srid (params, attr);
	  params->is_multi_point = 1;
	  return;
      }
    if (strcasecmp (el, "gml:LineString") == 0)
      {
	  set_srid (params, attr);
	  params->is_linestring = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiLineString") == 0)
      {
	  set_srid (params, attr);
	  params->is_multi_linestring = 1;
	  return;
      }
    if (strcasecmp (el, "gml:Polygon") == 0)
      {
	  set_srid (params, attr);
	  params->is_polygon = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiPolygon") == 0)
      {
	  set_srid (params, attr);
	  params->is_multi_polygon = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiSurface") == 0)
      {
	  set_srid (params, attr);
	  params->is_multi_polygon = 1;
	  return;
      }
    if (strcasecmp (el, "gml:featureMember") == 0)
      {
	  params->is_feature = 1;
	  return;
      }
    if (strcasecmp (el, "gml:featureMembers") == 0)
      {
	  params->is_feature = 1;
	  return;
      }
    if (params->is_fid)
      {
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
      }
    if (params->is_feature)
	check_start_fid (params, el, attr);
}

static void
end1_tag (void *data, const char *el)
{
/* GML element ending [Pass I] */
    struct gml_params *params = (struct gml_params *) data;
    if (strcmp (el, "gml:featureMember") == 0)
      {
	  params->is_feature = 0;
	  return;
      }
    if (strcmp (el, "gml:featureMembers") == 0)
      {
	  params->is_feature = 0;
	  return;
      }
    if (strcasecmp (el, "gml:Point") == 0)
      {
	  params->is_point = 0;
	  params->geometry_column = 1;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  set_geometry (params, GEOM_POINT);
	  return;
      }
    if (strcasecmp (el, "gml:MultiPoint") == 0)
      {
	  params->is_multi_point = 0;
	  params->geometry_column = 1;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  set_geometry (params, GEOM_MULTIPOINT);
	  return;
      }
    if (strcasecmp (el, "gml:LineString") == 0)
      {
	  params->is_linestring = 0;
	  params->geometry_column = 1;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  set_geometry (params, GEOM_LINESTRING);
	  return;
      }
    if (strcasecmp (el, "gml:MultiLineString") == 0)
      {
	  params->is_multi_linestring = 0;
	  params->geometry_column = 1;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  set_geometry (params, GEOM_MULTILINESTRING);
	  return;
      }
    if (strcasecmp (el, "gml:Polygon") == 0)
      {
	  params->is_polygon = 0;
	  params->geometry_column = 1;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  set_geometry (params, GEOM_MULTIPOLYGON);
	  return;
      }
    if (strcasecmp (el, "gml:MultiPolygon") == 0)
      {
	  params->is_multi_polygon = 0;
	  params->geometry_column = 1;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  set_geometry (params, GEOM_MULTIPOLYGON);
	  return;
      }
    if (strcasecmp (el, "gml:MultiSurface") == 0)
      {
	  params->is_multi_polygon = 0;
	  params->geometry_column = 1;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  set_geometry (params, GEOM_MULTIPOLYGON);
	  return;
      }
    if (params->is_feature)
	check_end1_fid (params, el);
    if (params->is_fid)
      {
	  *(params->CharData + params->CharDataLen) = '\0';
	  column_name (params, el);
      }
    *(params->CharData) = '\0';
    params->CharDataLen = 0;
}

static void
column_value (struct gml_params *params, const char *el)
{
/* handling a column value */
    char prefix[1024];
    char name[1024];
    struct gml_column *col;
    if (params->is_point || params->is_multi_point || params->is_linestring
	|| params->is_multi_linestring || params->is_polygon
	|| params->is_multi_polygon)
	return;
    split_fid (el, prefix, name);
    col = params->first;
    while (col)
      {
	  if (strcasecmp (col->name, name) == 0)
	    {
		if (params->CharDataLen == 0)
		    return;
		switch (col->type)
		  {
		  case COLUMN_INT:
		      col->int_value = atol_64 (params->CharData);
		      col->value_type = VALUE_INT;
		      break;
		  case COLUMN_FLOAT:
		      col->dbl_value = atof (params->CharData);
		      col->value_type = VALUE_FLOAT;
		      break;
		  case COLUMN_TEXT:
		      col->txt_value = malloc (params->CharDataLen + 1);
		      strcpy (col->txt_value, params->CharData);
		      col->value_type = VALUE_TEXT;
		      break;
		  }
		return;
	    }
	  col = col->next;
      }
}

static void
polygon_set_up (struct gml_params *params)
{
/* setting up the latest Polygon (if any) */
    int iv;
    int ib;
    int interiors;
    struct gml_rings_list *rg;
    gaiaDynamicLinePtr dyn_line;
    gaiaPointPtr pt;
    gaiaPolygonPtr pg;
    gaiaRingPtr ring;

    if (params->polygon.exterior == NULL)
	return;

    if (params->geometry == NULL)
	params->geometry = gaiaAllocGeomColl ();

/* preliminary recognition */
    dyn_line = params->polygon.exterior;
    iv = 0;
    pt = dyn_line->First;
    while (pt)
      {
	  /* counting how many POINTs are into the Exterior Ring */
	  iv++;
	  pt = pt->Next;
      }
    interiors = 0;
    rg = params->polygon.first;
    while (rg)
      {
	  /* counting how many Interior Rings are there */
	  interiors++;
	  rg = rg->next;
      }
    pg = gaiaAddPolygonToGeomColl (params->geometry, iv, interiors);

/* setting up the Exterior Ring */
    ring = pg->Exterior;
    iv = 0;
    dyn_line = params->polygon.exterior;
    pt = dyn_line->First;
    while (pt)
      {
	  /* inserting any POINT into the Exterior Ring */
	  gaiaSetPoint (ring->Coords, iv, pt->X, pt->Y);
	  iv++;
	  pt = pt->Next;
      }

    ib = 0;
    rg = params->polygon.first;
    while (rg)
      {
	  /* setting up any Interior Ring */
	  dyn_line = params->polygon.exterior;
	  iv = 0;
	  pt = dyn_line->First;
	  while (pt)
	    {
		/* counting how many POINTs are into the Interior Ring */
		iv++;
		pt = pt->Next;
	    }
	  ring = gaiaAddInteriorRing (pg, ib, iv);
	  iv = 0;
	  pt = dyn_line->First;
	  while (pt)
	    {
		/* inserting any POINT into the Interior Ring */
		gaiaSetPoint (ring->Coords, iv, pt->X, pt->Y);
		iv++;
		pt = pt->Next;
	    }
	  rg = rg->next;
	  ib++;
      }
}

static void
clean_polygon (struct gml_params *params)
{
/* cleaning up temporary Polygon struct */
    struct gml_rings_list *p;
    struct gml_rings_list *pn;
    if (params->polygon.exterior)
	gaiaFreeDynamicLine (params->polygon.exterior);
    p = params->polygon.first;
    while (p)
      {
	  pn = p->next;
	  free (p->ring);
	  free (p);
	  p = pn;
      }
    params->polygon.exterior = NULL;
    params->polygon.first = NULL;
    params->polygon.last = NULL;
}

static void
clean_geometry (struct gml_params *params)
{
/* cleaning up temporary Geometry struct */
    if (params->geometry)
	gaiaFreeGeomColl (params->geometry);
    params->geometry = NULL;
    clean_polygon (params);
}

static void
clean_values (struct gml_params *params)
{
/* cleaning row values */
    struct gml_column *col = params->first;
    while (col)
      {
	  col->value_type = VALUE_NULL;
	  if (col->txt_value)
	      free (col->txt_value);
	  col->txt_value = NULL;
	  col = col->next;
      }
    clean_geometry (params);
}

static void
check_end2_fid (struct gml_params *params, const char *el)
{
/* checking if the FID tag ends here */
    int ret;
    int fld = 1;
    struct gml_column *col;
    if (strcasecmp (params->fid_tag, el) == 0)
      {
	  params->is_fid = 0;
	  if (params->stmt == NULL)
	      return;

	  /* resetting the SQL prepared statement */
	  sqlite3_reset (params->stmt);
	  sqlite3_clear_bindings (params->stmt);
	  col = params->first;
	  while (col)
	    {
		/* binding ordinary column values */
		switch (col->value_type)
		  {
		  case VALUE_INT:
		      sqlite3_bind_int64 (params->stmt, fld, col->int_value);
		      break;
		  case VALUE_FLOAT:
		      sqlite3_bind_double (params->stmt, fld, col->dbl_value);
		      break;
		  case VALUE_TEXT:
		      sqlite3_bind_text (params->stmt, fld, col->txt_value,
					 strlen (col->txt_value),
					 SQLITE_STATIC);
		      break;
		  default:
		      sqlite3_bind_null (params->stmt, fld);
		      break;
		  };
		fld++;
		col = col->next;
	    }
	  /* setting up the latest Polygon (if any) */
	  polygon_set_up (params);
	  /* binding Geometry BLOB value */
	  if (params->geometry == NULL)
	      sqlite3_bind_null (params->stmt, fld);
	  else
	    {
		unsigned char *blob;
		int blob_size;
		params->geometry->Srid = params->srid;
		params->geometry->DeclaredType = params->declared_type;
		gaiaToSpatiaLiteBlobWkb (params->geometry, &blob, &blob_size);
		sqlite3_bind_blob (params->stmt, fld, blob, blob_size, free);
	    }
	  /* performing INSERT INTO */
	  ret = sqlite3_step (params->stmt);
	  clean_values (params);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      return;
	  fprintf (stderr, "sqlite3_step() error: INSERT INTO\n");
	  sqlite3_finalize (params->stmt);
	  params->stmt = NULL;
      }
}

static void
prepare_parse_coords (struct gml_params *params, const char **attr)
{
/* setting the SRID */
    int count = 0;
    const char *k;
    const char *v;
    const char **attrib = attr;

    while (*attrib != NULL)
      {
	  if ((count % 2) == 0)
	      k = *attrib;
	  else
	    {
		v = *attrib;
		if (strcasecmp (k, "decimal") == 0)
		    params->coord_decimal = *v;
		if (strcasecmp (k, "ts") == 0)
		    params->coord_ts = *v;
		if (strcasecmp (k, "cs") == 0)
		    params->coord_cs = *v;
	    }
	  attrib++;
	  count++;
      }
}

static void
add_point_to_geometry (struct gml_params *params, gaiaDynamicLinePtr dyn_line)
{
/* adding a POINT to current Geometry */
    gaiaPointPtr pt;

    if (params->geometry == NULL)
	params->geometry = gaiaAllocGeomColl ();

    pt = dyn_line->First;
    while (pt)
      {
	  /* inserting any POINT */
	  gaiaAddPointToGeomColl (params->geometry, pt->X, pt->Y);
	  pt = pt->Next;
      }
    gaiaFreeDynamicLine (dyn_line);
}

static void
add_linestring_to_geometry (struct gml_params *params,
			    gaiaDynamicLinePtr dyn_line)
{
/* adding a LINESTRING to current Geometry */
    int iv = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr line;

    if (params->geometry == NULL)
	params->geometry = gaiaAllocGeomColl ();

    pt = dyn_line->First;
    while (pt)
      {
	  /* counting how many POINTs are there */
	  iv++;
	  pt = pt->Next;
      }
    line = gaiaAddLinestringToGeomColl (params->geometry, iv);

    iv = 0;
    pt = dyn_line->First;
    while (pt)
      {
	  /* inserting any POINT into LINESTRING */
	  gaiaSetPoint (line->Coords, iv, pt->X, pt->Y);
	  iv++;
	  pt = pt->Next;
      }
    gaiaFreeDynamicLine (dyn_line);
}

static void
add_exterior_ring_to_geometry (struct gml_params *params,
			       gaiaDynamicLinePtr dyn_line)
{
/* adding an Exterior ring to the Polygon's temporary struct */
    if (params->polygon.exterior)
	gaiaFreeDynamicLine (params->polygon.exterior);
    params->polygon.exterior = dyn_line;
}

static void
add_interior_ring_to_geometry (struct gml_params *params,
			       gaiaDynamicLinePtr dyn_line)
{
/* adding an Interior ring to the Polygon's temporary struct */
    struct gml_rings_list *p = malloc (sizeof (struct gml_rings_list));
    p->ring = dyn_line;
    p->next = NULL;
    if (params->polygon.first == NULL)
	params->polygon.first = p;
    if (params->polygon.last != NULL)
	params->polygon.last->next = p;
    params->polygon.last = p;
}

static void
parse_coords_1 (struct gml_params *params)
{
/* parsing a list of coordinates (2) */
    int count = 0;
    char coord[128];
    double x;
    double y;
    char *p_out = coord;
    const char *p_in = params->CharData;
    int i;
    gaiaDynamicLinePtr dyn_line = gaiaAllocDynamicLine ();

    for (i = 0; i < params->CharDataLen; i++)
      {
	  if (*p_in == params->coord_ts || *p_in == params->coord_cs)
	    {
		*p_out = '\0';
		count++;
		if (params->coord_decimal != '.')
		  {
		      /* normalizing decimal separator */
		      int i;
		      for (i = 0; i < (int) strlen (coord); i++)
			{
			    if (coord[i] == params->coord_decimal)
				coord[i] = '.';
			}
		  }
		if (count == 1)
		    x = atof (coord);
		else
		    y = atof (coord);
		p_out = coord;
		p_in++;
		if (count > 1)
		  {
		      gaiaAppendPointToDynamicLine (dyn_line, x, y);
		      x = DBL_MAX;
		      y = DBL_MAX;
		      count = 0;
		  }
		continue;
	    }
	  *p_out++ = *p_in++;
      }
    if (p_out != coord)
      {
	  *p_out = '\0';
	  count++;
	  if (count == 1)
	      x = atof (coord);
	  else
	      y = atof (coord);
	  gaiaAppendPointToDynamicLine (dyn_line, x, y);
      }

    if (params->is_point)
	add_point_to_geometry (params, dyn_line);
    if (params->is_linestring)
	add_linestring_to_geometry (params, dyn_line);
    if (params->is_polygon)
      {
	  if (params->is_inner_boundary)
	      add_interior_ring_to_geometry (params, dyn_line);
	  else if (params->is_outer_boundary)
	      add_exterior_ring_to_geometry (params, dyn_line);
      }
}

static void
parse_coords_2 (struct gml_params *params)
{
/* parsing a list of coordinates (2) */
    int count = 0;
    char coord[128];
    double x;
    double y;
    char *p_out = coord;
    const char *p_in = params->CharData;
    int i;
    gaiaDynamicLinePtr dyn_line = gaiaAllocDynamicLine ();

    for (i = 0; i < params->CharDataLen; i++)
      {
	  if (*p_in == ' ')
	    {
		*p_out = '\0';
		count++;
		if (count == 1)
		    x = atof (coord);
		else
		    y = atof (coord);
		p_out = coord;
		p_in++;
		if (count > 1)
		  {
		      gaiaAppendPointToDynamicLine (dyn_line, x, y);
		      x = DBL_MAX;
		      y = DBL_MAX;
		      count = 0;
		  }
		continue;
	    }
	  *p_out++ = *p_in++;
      }
    if (p_out != coord)
      {
	  *p_out = '\0';
	  count++;
	  if (count == 1)
	      x = atof (coord);
	  else
	      y = atof (coord);
	  gaiaAppendPointToDynamicLine (dyn_line, x, y);
      }

    if (params->is_point)
	add_point_to_geometry (params, dyn_line);
    if (params->is_linestring)
	add_linestring_to_geometry (params, dyn_line);
    if (params->is_polygon)
      {
	  if (params->is_inner_boundary)
	      add_interior_ring_to_geometry (params, dyn_line);
	  else if (params->is_outer_boundary)
	      add_exterior_ring_to_geometry (params, dyn_line);
      }
}

static void
start2_tag (void *data, const char *el, const char **attr)
{
/* GML element starting [Pass II] */
    struct gml_params *params = (struct gml_params *) data;
    if (strcasecmp (el, "gml:coordinates") == 0)
	prepare_parse_coords (params, attr);
    if (strcasecmp (el, "gml:Point") == 0)
      {
	  params->is_point = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiPoint") == 0)
      {
	  params->is_multi_point = 1;
	  return;
      }
    if (strcasecmp (el, "gml:LineString") == 0)
      {
	  params->is_linestring = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiLineString") == 0)
      {
	  params->is_multi_linestring = 1;
	  return;
      }
    if (strcasecmp (el, "gml:Polygon") == 0)
      {
	  params->is_polygon = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiPolygon") == 0)
      {
	  params->is_multi_polygon = 1;
	  return;
      }
    if (strcasecmp (el, "gml:MultiSurface") == 0)
      {
	  params->is_multi_polygon = 1;
	  return;
      }
    if (strcasecmp (el, "gml:outerBoundaryIs") == 0)
      {
	  params->is_outer_boundary = 1;
	  return;
      }
    if (strcasecmp (el, "gml:exterior") == 0)
      {
	  params->is_outer_boundary = 1;
	  return;
      }
    if (strcasecmp (el, "gml:innerBoundaryIs") == 0)
      {
	  params->is_inner_boundary = 1;
	  return;
      }
    if (strcasecmp (el, "gml:interior") == 0)
      {
	  params->is_inner_boundary = 1;
	  return;
      }
    if (params->is_point || params->is_multi_point || params->is_linestring
	|| params->is_multi_linestring || params->is_polygon
	|| params->is_multi_polygon)
	return;
    if (strcasecmp (el, "gml:featureMember") == 0)
      {
	  params->is_feature = 1;
	  return;
      }
    if (strcasecmp (el, "gml:featureMember") == 0)
      {
	  clean_values (params);
	  params->is_feature = 1;
	  return;
      }
    if (strcasecmp (el, "gml:featureMembers") == 0)
      {
	  clean_values (params);
	  params->is_feature = 1;
	  return;
      }
    if (params->is_fid)
      {
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
      }
    if (params->is_feature)
	check_start_fid (params, el, attr);
}

static void
end2_tag (void *data, const char *el)
{
/* GML element ending [Pass II] */
    struct gml_params *params = (struct gml_params *) data;
    if (strcmp (el, "gml:featureMember") == 0)
      {
	  params->is_feature = 0;
	  return;
      }
    if (strcmp (el, "gml:featureMembers") == 0)
      {
	  params->is_feature = 0;
	  return;
      }
    if (strcasecmp (el, "gml:Point") == 0)
      {
	  params->is_point = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:MultiPoint") == 0)
      {
	  params->is_multi_point = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:LineString") == 0)
      {
	  params->is_linestring = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:MultiLineString") == 0)
      {
	  params->is_multi_linestring = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:Polygon") == 0)
      {
	  polygon_set_up (params);
	  clean_polygon (params);
	  params->is_polygon = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:MultiPolygon") == 0)
      {
	  params->is_multi_polygon = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:MultiSurface") == 0)
      {
	  params->is_multi_polygon = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:outerBoundaryIs") == 0)
      {
	  params->is_outer_boundary = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:exterior") == 0)
      {
	  params->is_outer_boundary = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:innerBoundaryIs") == 0)
      {
	  params->is_inner_boundary = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (strcasecmp (el, "gml:interior") == 0)
      {
	  params->is_inner_boundary = 0;
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (params->is_feature)
	check_end2_fid (params, el);
    if (params->is_fid)
      {
	  *(params->CharData + params->CharDataLen) = '\0';
	  column_value (params, el);
      }
    if (strcasecmp (el, "gml:coordinates") == 0)
	parse_coords_1 (params);
    if (strcasecmp (el, "gml:posList") == 0 || strcasecmp (el, "gml:pos") == 0)
	parse_coords_2 (params);
    *(params->CharData) = '\0';
    params->CharDataLen = 0;
}

static void
spatialite_autocreate (sqlite3 * db)
{
/* attempting to perform self-initialization for a newly created DB */
    int ret;
    char sql[1024];
    char *err_msg = NULL;
    int count;
    int i;
    char **results;
    int rows;
    int columns;

/* checking if this DB is really empty */
    strcpy (sql, "SELECT Count(*) from sqlite_master");
    ret = sqlite3_get_table (db, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	return;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	      count = atoi (results[(i * columns) + 0]);
      }
    sqlite3_free_table (results);

    if (count > 0)
	return;

/* all right, it's empty: proceding to initialize */
    strcpy (sql, "SELECT InitSpatialMetadata(1)");
    ret = sqlite3_exec (db, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "InitSpatialMetadata() error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return;
      }
}

static void
open_db (const char *path, sqlite3 ** handle, void *cache)
{
/* opening the DB */
    sqlite3 *db_handle;
    int ret;
    char sql[1024];
    int spatialite_rs = 0;
    int spatialite_gc = 0;
    int rs_srid = 0;
    int auth_name = 0;
    int auth_srid = 0;
    int ref_sys_name = 0;
    int proj4text = 0;
    int f_table_name = 0;
    int f_geometry_column = 0;
    int coord_dimension = 0;
    int gc_srid = 0;
    int type = 0;
    int geometry_type = 0;
    int spatial_index_enabled = 0;
    const char *name;
    int i;
    char **results;
    int rows;
    int columns;

    *handle = NULL;
    printf ("SQLite version: %s\n", sqlite3_libversion ());
    printf ("SpatiaLite version: %s\n\n", spatialite_version ());

    ret =
	sqlite3_open_v2 (path, &db_handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", path,
		   sqlite3_errmsg (db_handle));
	  sqlite3_close (db_handle);
	  db_handle = NULL;
	  return;
      }
    spatialite_init_ex (db_handle, cache, 0);
    spatialite_autocreate (db_handle);

/* checking the GEOMETRY_COLUMNS table */
    strcpy (sql, "PRAGMA table_info(geometry_columns)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "f_table_name") == 0)
		    f_table_name = 1;
		if (strcasecmp (name, "f_geometry_column") == 0)
		    f_geometry_column = 1;
		if (strcasecmp (name, "coord_dimension") == 0)
		    coord_dimension = 1;
		if (strcasecmp (name, "srid") == 0)
		    gc_srid = 1;
		if (strcasecmp (name, "type") == 0)
		    type = 1;
		if (strcasecmp (name, "geometry_type") == 0)
		    geometry_type = 1;
		if (strcasecmp (name, "spatial_index_enabled") == 0)
		    spatial_index_enabled = 1;
	    }
      }
    sqlite3_free_table (results);
    if (f_table_name && f_geometry_column && type && coord_dimension
	&& gc_srid && spatial_index_enabled)
	spatialite_gc = 1;
    if (f_table_name && f_geometry_column && geometry_type && coord_dimension
	&& gc_srid && spatial_index_enabled)
	spatialite_gc = 3;

/* checking the SPATIAL_REF_SYS table */
    strcpy (sql, "PRAGMA table_info(spatial_ref_sys)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "srid") == 0)
		    rs_srid = 1;
		if (strcasecmp (name, "auth_name") == 0)
		    auth_name = 1;
		if (strcasecmp (name, "auth_srid") == 0)
		    auth_srid = 1;
		if (strcasecmp (name, "ref_sys_name") == 0)
		    ref_sys_name = 1;
		if (strcasecmp (name, "proj4text") == 0)
		    proj4text = 1;
	    }
      }
    sqlite3_free_table (results);
    if (rs_srid && auth_name && auth_srid && ref_sys_name && proj4text)
	spatialite_rs = 1;
/* verifying the MetaData format */
    if (spatialite_gc && spatialite_rs)
	;
    else
	goto unknown;

    *handle = db_handle;
    return;

  unknown:
    if (db_handle)
	sqlite3_close (db_handle);
    fprintf (stderr, "DB '%s'\n", path);
    fprintf (stderr, "doesn't seems to contain valid Spatial Metadata ...\n\n");
    fprintf (stderr, "Please, initialize Spatial Metadata\n\n");
    return;
}

static int
create_table (struct gml_params *params, const char *table)
{
/* attempting to create the DB table */
    struct gml_column *col;
    char sql[4192];
    char sql2[1024];
    int ret;
    char *err_msg = NULL;
    sqlite3_stmt *stmt;

/* CREATE TABLE SQL statement */
    sprintf (sql, "CREATE TABLE %s (\n", table);
    strcat (sql, "PkUID INTEGER PRIMARY KEY AUTOINCREMENT");
    col = params->first;
    while (col)
      {
	  switch (col->type)
	    {
	    case COLUMN_INT:
		sprintf (sql2, ",\n%s INTEGER", col->name);
		break;
	    case COLUMN_FLOAT:
		sprintf (sql2, ",\n%s DOUBLE", col->name);
		break;
	    default:
		sprintf (sql2, ",\n%s TEXT", col->name);
		break;
	    };
	  if (*sql2 != '\0')
	    {
		strcat (sql, sql2);
	    }
	  col = col->next;
      }
    strcat (sql, "\n)");

    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

    sprintf (sql, "SELECT AddGeometryColumn('%s', 'geometry', %d", table,
	     params->srid);
    switch (params->geometry_type)
      {
      case GEOM_POINT:
	  strcat (sql, ", 'POINT'");
	  params->declared_type = GAIA_POINT;
	  break;
      case GEOM_MULTIPOINT:
	  strcat (sql, ", 'MULTIPOINT'");
	  params->declared_type = GAIA_MULTIPOINT;
	  break;
      case GEOM_LINESTRING:
	  strcat (sql, ", 'LINESTRING'");
	  params->declared_type = GAIA_LINESTRING;
	  break;
      case GEOM_MULTILINESTRING:
	  strcat (sql, ", 'MULTILINESTRING'");
	  params->declared_type = GAIA_MULTILINESTRING;
	  break;
      case GEOM_POLYGON:
	  strcat (sql, ", 'POLYGON'");
	  params->declared_type = GAIA_POLYGON;
	  break;
      case GEOM_MULTIPOLYGON:
	  strcat (sql, ", 'MULTIPOLYGON'");
	  params->declared_type = GAIA_MULTIPOLYGON;
	  break;
      default:
	  strcat (sql, ", 'GEOMETRYCOLLECTION'");
	  params->declared_type = GAIA_GEOMETRYCOLLECTION;
	  break;
      }
    strcat (sql, ", 'XY')");

    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "ADD GEOMETRY COLUMN '%s' error: %s\n", table,
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* creating a prepared statemet for INSERST INTO */
    sprintf (sql, "INSERT INTO %s (", table);
    strcat (sql, "PkUID");
    col = params->first;
    while (col)
      {
	  sprintf (sql2, ", %s", col->name);
	  strcat (sql, sql2);
	  col = col->next;
      }
    strcat (sql, ", geometry) VALUES (NULL");
    col = params->first;
    while (col)
      {
	  strcat (sql, ", ?");
	  col = col->next;
      }
    strcat (sql, ", ?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  return 0;
      }
    params->stmt = stmt;
/* the complete data load is handled as an unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

    return 1;
}

static void
free_columns (struct gml_params *params)
{
/* cleaning up the columns list */
    struct gml_column *col;
    struct gml_column *col_n;
    col = params->first;
    while (col)
      {
	  col_n = col->next;
	  free (col->name);
	  free (col);
	  if (col->txt_value)
	      free (col->txt_value);
	  col = col_n;
      }
    if (params->CharData)
	free (params->CharData);
    clean_geometry (params);
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_gml ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr, "-g or --gml-path pathname       the GML-XML file path\n");
    fprintf (stderr,
	     "-d or --db-path     pathname    the SpatiaLite DB path\n\n");
    fprintf (stderr, "-t or --table-name  name        the DB table name\n\n");
    fprintf (stderr, "you can specify the following options as well\n");
    fprintf (stderr,
	     "-m or --in-memory               using IN-MEMORY database\n");
    fprintf (stderr,
	     "-n or --no-spatial-index        suppress R*Tree generation\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    sqlite3 *handle;
    int i;
    int next_arg = ARG_NONE;
    const char *gml_path = NULL;
    const char *db_path = NULL;
    const char *table = NULL;
    int in_memory = 0;
    int error = 0;
    char Buff[BUFFSIZE];
    int done = 0;
    int len;
    XML_Parser parser;
    FILE *xml_file;
    struct gml_params params;
    int ret;
    char *err_msg = NULL;
    void *cache;

    params.db_handle = NULL;
    params.stmt = NULL;
    params.geometry_type = GEOM_NONE;
    params.srid = INT_MIN;
    params.is_feature = 0;
    params.is_fid = 0;
    params.is_point = 0;
    params.is_linestring = 0;
    params.is_polygon = 0;
    params.is_multi_point = 0;
    params.is_multi_linestring = 0;
    params.is_multi_polygon = 0;
    params.first = NULL;
    params.last = NULL;
    params.geometry = NULL;
    params.polygon.exterior = NULL;
    params.polygon.first = NULL;
    params.polygon.last = NULL;
    params.CharDataStep = 65536;
    params.CharDataMax = params.CharDataStep;
    params.CharData = malloc (params.CharDataStep);
    params.CharDataLen = 0;

    for (i = 1; i < argc; i++)
      {
	  /* parsing the invocation arguments */
	  if (next_arg != ARG_NONE)
	    {
		switch (next_arg)
		  {
		  case ARG_GML_PATH:
		      gml_path = argv[i];
		      break;
		  case ARG_DB_PATH:
		      db_path = argv[i];
		      break;
		  case ARG_TABLE:
		      table = argv[i];
		      break;
		  };
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--help") == 0
	      || strcmp (argv[i], "-h") == 0)
	    {
		do_help ();
		return -1;
	    }
	  if (strcmp (argv[i], "-g") == 0)
	    {
		next_arg = ARG_GML_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--gml-path") == 0)
	    {
		next_arg = ARG_GML_PATH;
		continue;
	    }
	  if (strcmp (argv[i], "-d") == 0)
	    {
		next_arg = ARG_DB_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--db-path") == 0)
	    {
		next_arg = ARG_DB_PATH;
		continue;
	    }
	  if (strcmp (argv[i], "-t") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--table-name") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "-m") == 0)
	    {
		in_memory = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "-in-memory") == 0)
	    {
		in_memory = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "-n") == 0)
	    {
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "-no-spatial-index") == 0)
	    {
		next_arg = ARG_NONE;
		continue;
	    }
	  fprintf (stderr, "unknown argument: %s\n", argv[i]);
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }

/* checking the arguments */
    if (!gml_path)
      {
	  fprintf (stderr,
		   "did you forget setting the --gml-path argument ?\n");
	  error = 1;
      }
    if (!db_path)
      {
	  fprintf (stderr, "did you forget setting the --db-path argument ?\n");
	  error = 1;
      }
    if (!table)
      {
	  fprintf (stderr,
		   "did you forget setting the --table-name argument ?\n");
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }

/* opening the DB */
    cache = spatialite_alloc_connection ();
    open_db (db_path, &handle, cache);
    if (!handle)
	return -1;
    params.db_handle = handle;
    if (in_memory)
      {
	  /* loading the DB in-memory */
	  sqlite3 *mem_db_handle;
	  sqlite3_backup *backup;
	  int ret;
	  ret =
	      sqlite3_open_v2 (":memory:", &mem_db_handle,
			       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
			       NULL);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "cannot open 'MEMORY-DB': %s\n",
			 sqlite3_errmsg (mem_db_handle));
		sqlite3_close (mem_db_handle);
		return -1;
	    }
	  backup = sqlite3_backup_init (mem_db_handle, "main", handle, "main");
	  if (!backup)
	    {
		fprintf (stderr, "cannot load 'MEMORY-DB'\n");
		sqlite3_close (handle);
		sqlite3_close (mem_db_handle);
		return -1;
	    }
	  while (1)
	    {
		ret = sqlite3_backup_step (backup, 1024);
		if (ret == SQLITE_DONE)
		    break;
	    }
	  ret = sqlite3_backup_finish (backup);
	  sqlite3_close (handle);
	  handle = mem_db_handle;
	  printf ("\nusing IN-MEMORY database\n");
	  spatialite_cleanup_ex (cache);
	  cache = spatialite_alloc_connection ();
	  spatialite_init_ex (handle, cache, 0);
      }

/* XML parsing */
    xml_file = fopen (gml_path, "rb");
    if (!xml_file)
      {
	  fprintf (stderr, "cannot open %s\n", gml_path);
	  sqlite3_close (handle);
	  return -1;
      }
    parser = XML_ParserCreate (NULL);
    if (!parser)
      {
	  fprintf (stderr, "Couldn't allocate memory for parser\n");
	  sqlite3_close (handle);
	  return -1;
      }

    XML_SetUserData (parser, &params);
/* XML parsing: pass I */
    XML_SetElementHandler (parser, start1_tag, end1_tag);
    XML_SetCharacterDataHandler (parser, gmlCharData);
    while (!done)
      {
	  len = fread (Buff, 1, BUFFSIZE, xml_file);
	  if (ferror (xml_file))
	    {
		fprintf (stderr, "XML Read error\n");
		sqlite3_close (handle);
		return -1;
	    }
	  done = feof (xml_file);
	  if (!XML_Parse (parser, Buff, len, done))
	    {
		fprintf (stderr, "Parse error at line %d:\n%s\n",
			 (int) XML_GetCurrentLineNumber (parser),
			 XML_ErrorString (XML_GetErrorCode (parser)));
		sqlite3_close (handle);
		return -1;
	    }
      }
    XML_ParserFree (parser);

/* creating the DB table */
    if (!create_table (&params, table))
	goto no_table;

/* XML parsing: pass II */
    parser = XML_ParserCreate (NULL);
    if (!parser)
      {
	  fprintf (stderr, "Couldn't allocate memory for parser\n");
	  sqlite3_close (handle);
	  return -1;
      }
    XML_SetUserData (parser, &params);
    XML_SetElementHandler (parser, start2_tag, end2_tag);
    XML_SetCharacterDataHandler (parser, gmlCharData);
    rewind (xml_file);
    params.is_feature = 0;
    params.is_fid = 0;
    params.is_point = 0;
    params.is_linestring = 0;
    params.is_polygon = 0;
    params.is_multi_point = 0;
    params.is_multi_linestring = 0;
    params.is_multi_polygon = 0;
    done = 0;
    while (!done)
      {
	  len = fread (Buff, 1, BUFFSIZE, xml_file);
	  if (ferror (xml_file))
	    {
		fprintf (stderr, "XML Read error\n");
		sqlite3_close (handle);
		return -1;
	    }
	  done = feof (xml_file);
	  if (!XML_Parse (parser, Buff, len, done))
	    {
		fprintf (stderr, "Parse error at line %d:\n%s\n",
			 (int) XML_GetCurrentLineNumber (parser),
			 XML_ErrorString (XML_GetErrorCode (parser)));
		sqlite3_close (handle);
		return -1;
	    }
      }
    XML_ParserFree (parser);
    if (params.stmt != NULL)
	sqlite3_finalize (params.stmt);
    params.stmt = NULL;

    /* COMMITTing the still pending Transaction */
    ret = sqlite3_exec (handle, "COMMIT", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

  no_table:
    free_columns (&params);
    fclose (xml_file);

    if (in_memory)
      {
	  /* exporting the in-memory DB to filesystem */
	  sqlite3 *disk_db_handle;
	  sqlite3_backup *backup;
	  int ret;
	  printf ("\nexporting IN_MEMORY database ... wait please ...\n");
	  ret =
	      sqlite3_open_v2 (db_path, &disk_db_handle,
			       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
			       NULL);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "cannot open '%s': %s\n", db_path,
			 sqlite3_errmsg (disk_db_handle));
		sqlite3_close (disk_db_handle);
		return -1;
	    }
	  backup = sqlite3_backup_init (disk_db_handle, "main", handle, "main");
	  if (!backup)
	    {
		fprintf (stderr, "Backup failure: 'MEMORY-DB' wasn't saved\n");
		sqlite3_close (handle);
		sqlite3_close (disk_db_handle);
		return -1;
	    }
	  while (1)
	    {
		ret = sqlite3_backup_step (backup, 1024);
		if (ret == SQLITE_DONE)
		    break;
	    }
	  ret = sqlite3_backup_finish (backup);
	  sqlite3_close (handle);
	  handle = disk_db_handle;
	  printf ("\tIN_MEMORY database successfully exported\n");
      }

    sqlite3_close (handle);
    spatialite_cleanup_ex (cache);
    spatialite_shutdown ();
    return 0;
}
