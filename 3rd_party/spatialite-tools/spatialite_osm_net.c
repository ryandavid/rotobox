/* 
/ spatialite_osm_net
/
/ a tool loading OSM-XML roads into a SpatiaLite DB
/
/ version 1.0, 2009 August 1
/
/ Author: Sandro Furieri a.furieri@lqt.it
/
/ Copyright (C) 2009  Alessandro Furieri
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
#include <readosm.h>

#ifdef _WIN32
#define strcasecmp	_stricmp
#endif /* not WIN32 */

#define ARG_NONE		0
#define ARG_OSM_PATH	1
#define ARG_DB_PATH		2
#define ARG_TABLE		3
#define ARG_CACHE_SIZE	4
#define ARG_TEMPLATE_PATH	5

#define NODE_STRAT_NONE	0
#define NODE_STRAT_ALL	1
#define NODE_STRAT_ENDS	2
#define ONEWAY_STRAT_NONE	0
#define ONEWAY_STRAT_FULL	1
#define ONEWAY_STRAT_NO_ROUND	2
#define ONEWAY_STRAT_NO_MOTOR	3
#define ONEWAY_STRAT_NO_BOTH	4

struct aux_speed
{
/* an auxiliary struct for Speeds */
    char *class_name;
    double speed;
    struct aux_speed *next;
};

struct aux_class
{
/* an auxiliary struct for road Classes */
    char *class_name;
    char *sub_class;
    struct aux_class *next;
};

struct aux_params
{
/* an auxiliary struct used for OSM parsing */
    sqlite3 *db_handle;
    const char *table;
    int double_arcs;
    int noding_strategy;
    int oneway_strategy;
    struct aux_speed *first_speed;
    struct aux_speed *last_speed;
    double default_speed;
    struct aux_class *first_include;
    struct aux_class *last_include;
    struct aux_class *first_ignore;
    struct aux_class *last_ignore;
    sqlite3_stmt *ins_tmp_nodes_stmt;
    sqlite3_stmt *upd_tmp_nodes_stmt;
    sqlite3_stmt *rd_tmp_nodes_stmt;
    sqlite3_stmt *ins_arcs_stmt;
};

static void
save_current_line (gaiaGeomCollPtr geom, gaiaDynamicLinePtr dyn)
{
/* inserting a GraphArc from a splitted Way */
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    int iv;
    int points = 0;

    pt = dyn->First;
    while (pt)
      {
	  /* counting how many points are there */
	  points++;
	  pt = pt->Next;
      }
    if (points < 2)
	return;
    ln = gaiaAddLinestringToGeomColl (geom, points);
    iv = 0;
    pt = dyn->First;
    while (pt)
      {
	  gaiaSetPoint (ln->Coords, iv, pt->X, pt->Y);
	  iv++;
	  pt = pt->Next;
      }
}

static gaiaGeomCollPtr
build_linestrings (struct aux_params *params, const readosm_way * way)
{
/* building the Arcs of the Graph [may be, splitting a Way in more Arcs] */
    int i_ref;
    int ret;
    gaiaGeomCollPtr geom;
    gaiaDynamicLinePtr dyn = gaiaAllocDynamicLine ();
    gaiaPointPtr pt;

    geom = gaiaAllocGeomColl ();
    geom->Srid = 4326;

    for (i_ref = 0; i_ref < way->node_ref_count; i_ref++)
      {
	  /* fetching point coords */
	  sqlite3_int64 id = *(way->node_refs + i_ref);
	  double x;
	  double y;
	  int ref_count;

	  sqlite3_reset (params->rd_tmp_nodes_stmt);
	  sqlite3_clear_bindings (params->rd_tmp_nodes_stmt);
	  sqlite3_bind_int64 (params->rd_tmp_nodes_stmt, 1, id);

	  /* scrolling the result set */
	  ret = sqlite3_step (params->rd_tmp_nodes_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* empty resultset - no coords !!! */
#if defined(_WIN32) || defined(__MINGW32__)
		/* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
		fprintf (stderr, "UNRESOLVED-NODE %I64d\n", id);
#else
		fprintf (stderr, "UNRESOLVED-NODE %lld\n", id);
#endif
		gaiaFreeDynamicLine (dyn);
		gaiaFreeGeomColl (geom);
		return NULL;
	    }
	  else if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		x = sqlite3_column_double (params->rd_tmp_nodes_stmt, 0);
		y = sqlite3_column_double (params->rd_tmp_nodes_stmt, 1);
		ref_count = sqlite3_column_int (params->rd_tmp_nodes_stmt, 2);

		pt = dyn->Last;
		if (pt)
		  {
		      if (pt->X == x && pt->Y == y)
			{
			    /* skipping any repeated point */
			    continue;
			}
		  }

		/* appending the point to the current line anyway */
		gaiaAppendPointToDynamicLine (dyn, x, y);
		if (params->noding_strategy != NODE_STRAT_NONE)
		  {
		      /* attempting to renode the Graph */
		      int limit = 1;
		      if (params->noding_strategy == NODE_STRAT_ENDS)
			{
			    /* renoding each Way terminal point */
			    limit = 0;
			}
		      if ((ref_count > limit)
			  && (i_ref > 0 && i_ref < (way->node_ref_count - 1)))
			{
			    /* found an internal Node: saving the current line */
			    save_current_line (geom, dyn);
			    /* starting a further line */
			    gaiaFreeDynamicLine (dyn);
			    dyn = gaiaAllocDynamicLine ();
			    /* inserting the current point in the new line */
			    gaiaAppendPointToDynamicLine (dyn, x, y);
			}
		  }
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		sqlite3_finalize (params->rd_tmp_nodes_stmt);
		gaiaFreeGeomColl (geom);
		gaiaFreeDynamicLine (dyn);
		return NULL;
	    }
      }
/* saving the last line */
    save_current_line (geom, dyn);
    gaiaFreeDynamicLine (dyn);

    if (geom->FirstLinestring == NULL)
      {
	  /* invalid geometry: no lines */
	  gaiaFreeGeomColl (geom);
	  return NULL;
      }
    return geom;
}

static int
arcs_insert_straight (struct aux_params *params, sqlite3_int64 id,
		      const char *class, const char *name, unsigned char *blob,
		      int blob_size)
{
/* Inserts a uniderectional Arc into the Graph - straight direction */
    int ret;
    if (params->ins_arcs_stmt == NULL)
	return 1;
    sqlite3_reset (params->ins_arcs_stmt);
    sqlite3_clear_bindings (params->ins_arcs_stmt);
    sqlite3_bind_int64 (params->ins_arcs_stmt, 1, id);
    sqlite3_bind_text (params->ins_arcs_stmt, 2, class, strlen (class),
		       SQLITE_STATIC);
    sqlite3_bind_text (params->ins_arcs_stmt, 3, name, strlen (name),
		       SQLITE_STATIC);
    sqlite3_bind_blob (params->ins_arcs_stmt, 4, blob, blob_size,
		       SQLITE_STATIC);
    ret = sqlite3_step (params->ins_arcs_stmt);

    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	return 1;
    fprintf (stderr, "sqlite3_step() error: INS_ARCS\n");
    sqlite3_finalize (params->ins_arcs_stmt);
    params->ins_arcs_stmt = NULL;
    return 0;
}

static int
arcs_insert_reverse (struct aux_params *params, sqlite3_int64 id,
		     const char *class, const char *name, unsigned char *blob,
		     int blob_size)
{
/* Inserts a unidirectional Arc into the Graph - reverse direction */
    int ret;
    gaiaGeomCollPtr g1;
    gaiaGeomCollPtr g2;
    gaiaLinestringPtr ln1;
    gaiaLinestringPtr ln2;
    unsigned char *blob2;
    int blob_size2;
    int iv1;
    int iv2;
    double x;
    double y;
    double z;
    double m;
    if (params->ins_arcs_stmt == NULL)
	return 1;

/* creating a geometry in reverse order */
    g1 = gaiaFromSpatiaLiteBlobWkb (blob, blob_size);
    if (!g1)
	return 0;
    ln1 = g1->FirstLinestring;
    if (!ln1)
	return 0;
    if (g1->DimensionModel == GAIA_XY_Z)
	g2 = gaiaAllocGeomCollXYZ ();
    else if (g1->DimensionModel == GAIA_XY_M)
	g2 = gaiaAllocGeomCollXYM ();
    else if (g1->DimensionModel == GAIA_XY_Z_M)
	g2 = gaiaAllocGeomCollXYZM ();
    else
	g2 = gaiaAllocGeomColl ();
    g2->Srid = g1->Srid;
    ln2 = gaiaAddLinestringToGeomColl (g2, ln1->Points);
    iv2 = ln1->Points - 1;
    for (iv1 = 0; iv1 < ln1->Points; iv1++)
      {
	  /* copying points in reverse order */
	  if (g1->DimensionModel == GAIA_XY_Z)
	    {
		gaiaGetPointXYZ (ln1->Coords, iv1, &x, &y, &z);
	    }
	  else if (g1->DimensionModel == GAIA_XY_M)
	    {
		gaiaGetPointXYM (ln1->Coords, iv1, &x, &y, &m);
	    }
	  else if (g1->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaGetPointXYZM (ln1->Coords, iv1, &x, &y, &z, &m);
	    }
	  else
	    {
		gaiaGetPoint (ln1->Coords, iv1, &x, &y);
	    }
	  if (g2->DimensionModel == GAIA_XY_Z)
	    {
		gaiaSetPointXYZ (ln2->Coords, iv2, x, y, z);
	    }
	  else if (g2->DimensionModel == GAIA_XY_M)
	    {
		gaiaSetPointXYM (ln2->Coords, iv2, x, y, m);
	    }
	  else if (g2->DimensionModel == GAIA_XY_Z_M)
	    {
		gaiaSetPointXYZM (ln2->Coords, iv2, x, y, z, m);
	    }
	  else
	    {
		gaiaSetPoint (ln2->Coords, iv2, x, y);
	    }
	  iv2--;
      }
    gaiaToSpatiaLiteBlobWkb (g2, &blob2, &blob_size2);
    gaiaFreeGeomColl (g1);
    gaiaFreeGeomColl (g2);

    sqlite3_reset (params->ins_arcs_stmt);
    sqlite3_clear_bindings (params->ins_arcs_stmt);
    sqlite3_bind_int64 (params->ins_arcs_stmt, 1, id);
    sqlite3_bind_text (params->ins_arcs_stmt, 2, class, strlen (class),
		       SQLITE_STATIC);
    sqlite3_bind_text (params->ins_arcs_stmt, 3, name, strlen (name),
		       SQLITE_STATIC);
    sqlite3_bind_blob (params->ins_arcs_stmt, 4, blob2, blob_size2, free);
    ret = sqlite3_step (params->ins_arcs_stmt);

    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	return 1;
    fprintf (stderr, "sqlite3_step() error: INS_ARCS\n");
    sqlite3_finalize (params->ins_arcs_stmt);
    params->ins_arcs_stmt = NULL;
    return 0;
}

static int
arcs_insert_double (struct aux_params *params, sqlite3_int64 id,
		    const char *class, const char *name, int oneway,
		    unsigned char *blob, int blob_size)
{
/* Inserts a unidirectional Arc into the Graph */
    if (oneway >= 0)
      {
	  /* inserting the straight direction */
	  if (!arcs_insert_straight (params, id, class, name, blob, blob_size))
	      goto stop;
      }
    if (oneway <= 0)
      {
	  /* inserting the reverse direction */
	  if (!arcs_insert_reverse (params, id, class, name, blob, blob_size))
	      goto stop;
      }
    if (blob)
	free (blob);
    return 1;

  stop:
    if (blob)
	free (blob);
    return 0;
}

static int
arcs_insert (struct aux_params *params, sqlite3_int64 id,
	     const char *class, const char *name, int oneway,
	     unsigned char *blob, int blob_size)
{
/* Inserts a bi-directional Arc into the Graph */
    int ret;
    if (params->ins_arcs_stmt == NULL)
	return 1;
    sqlite3_reset (params->ins_arcs_stmt);
    sqlite3_clear_bindings (params->ins_arcs_stmt);
    sqlite3_bind_int64 (params->ins_arcs_stmt, 1, id);
    sqlite3_bind_text (params->ins_arcs_stmt, 2, class, strlen (class),
		       SQLITE_STATIC);
    sqlite3_bind_text (params->ins_arcs_stmt, 3, name, strlen (name),
		       SQLITE_STATIC);
    if (oneway > 0)
      {
	  sqlite3_bind_int (params->ins_arcs_stmt, 4, 1);
	  sqlite3_bind_int (params->ins_arcs_stmt, 5, 0);
      }
    else if (oneway < 0)
      {
	  sqlite3_bind_int (params->ins_arcs_stmt, 4, 0);
	  sqlite3_bind_int (params->ins_arcs_stmt, 5, 1);
      }
    else
      {
	  sqlite3_bind_int (params->ins_arcs_stmt, 4, 1);
	  sqlite3_bind_int (params->ins_arcs_stmt, 5, 1);
      }
    sqlite3_bind_blob (params->ins_arcs_stmt, 6, blob, blob_size, free);
    ret = sqlite3_step (params->ins_arcs_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	return 1;
    fprintf (stderr, "sqlite3_step() error: INS_ARCS\n");
    sqlite3_finalize (params->ins_arcs_stmt);
    params->ins_arcs_stmt = NULL;
    return 0;
}

static int
find_include_class (struct aux_params *params, const char *class,
		    const char *sub_class)
{
/* testing if this Way belongs to some class to be Included */
    struct aux_class *pc = params->first_include;
    while (pc)
      {
	  if (strcmp (pc->class_name, class) == 0)
	    {
		if (pc->sub_class == NULL)
		    return 1;
		if (strcmp (pc->sub_class, sub_class) == 0)
		    return 1;
	    }
	  pc = pc->next;
      }
    return 0;
}

static int
find_ignore_class (struct aux_params *params, const char *class,
		   const char *sub_class)
{
/* testing if this Way belongs to some class to be Ignored */
    struct aux_class *pc = params->first_ignore;
    while (pc)
      {
	  if (strcmp (pc->class_name, class) == 0)
	    {
		if (strcmp (pc->sub_class, sub_class) == 0)
		    return 1;
	    }
	  pc = pc->next;
      }
    return 0;
}

static int
consume_way_1 (const void *user_data, const readosm_way * way)
{
/* processing an OSM Way - Pass#1 (ReadOSM callback function) */
    struct aux_params *params = (struct aux_params *) user_data;
    const readosm_tag *p_tag;
    int i_tag;
    int i_ref;
    int ret;
    sqlite3_int64 id;
    int include = 0;
    int ignore = 0;
    if (params->noding_strategy == NODE_STRAT_NONE)
      {
	  /* renoding the graph isn't required, we can skip all this */
	  return READOSM_OK;
      }

    for (i_tag = 0; i_tag < way->tag_count; i_tag++)
      {
	  p_tag = way->tags + i_tag;
	  if (find_include_class (params, p_tag->key, p_tag->value))
	      include = 1;
	  if (find_ignore_class (params, p_tag->key, p_tag->value))
	      ignore = 1;
      }
    if (!include || ignore)
	return READOSM_OK;
    for (i_ref = 0; i_ref < way->node_ref_count; i_ref++)
      {
	  if (params->noding_strategy == NODE_STRAT_ENDS)
	    {
		/* checking only the Way extreme points */
		if (i_ref == 0 || i_ref == (way->node_ref_count - 1));
		else
		    continue;
	    }
	  id = *(way->node_refs + i_ref);
	  sqlite3_reset (params->upd_tmp_nodes_stmt);
	  sqlite3_clear_bindings (params->upd_tmp_nodes_stmt);
	  sqlite3_bind_int64 (params->upd_tmp_nodes_stmt, 1, id);
	  ret = sqlite3_step (params->upd_tmp_nodes_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW);
	  else
	    {
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		sqlite3_finalize (params->upd_tmp_nodes_stmt);
		return READOSM_ABORT;
	    }
      }

    return READOSM_OK;
}

static int
consume_way_2 (const void *user_data, const readosm_way * way)
{
/* processing an OSM Way - Pass#2 (ReadOSM callback function) */
    struct aux_params *params = (struct aux_params *) user_data;
    const readosm_tag *p_tag;
    int i_tag;
    unsigned char *blob;
    int blob_size;
    const char *class = NULL;
    const char *name = "*** Unknown ****";
    int oneway = 0;
    int ret;
    int include = 0;
    int ignore = 0;
    gaiaGeomCollPtr geom;
    for (i_tag = 0; i_tag < way->tag_count; i_tag++)
      {
	  p_tag = way->tags + i_tag;
	  if (find_include_class (params, p_tag->key, p_tag->value))
	    {
		include = 1;
		class = p_tag->value;
	    }
	  if (find_ignore_class (params, p_tag->key, p_tag->value))
	      ignore = 1;
      }
    if (!include || ignore)
	return READOSM_OK;
    for (i_tag = 0; i_tag < way->tag_count; i_tag++)
      {
	  /* retrieving the road name */
	  p_tag = way->tags + i_tag;
	  if (strcmp (p_tag->key, "name") == 0)
	    {
		name = p_tag->value;
		break;
	    }
      }

    if (params->oneway_strategy != ONEWAY_STRAT_NONE)
      {
	  /* checking for Oneeays */
	  for (i_tag = 0; i_tag < way->tag_count; i_tag++)
	    {
		/* checking for one-ways */
		p_tag = way->tags + i_tag;
		if (strcmp (p_tag->key, "oneway") == 0)
		  {
		      if (strcmp (p_tag->value, "yes") == 0
			  || strcmp (p_tag->value, "true") == 0
			  || strcmp (p_tag->value, "1") == 0)
			  oneway = 1;
		      if (strcmp (p_tag->value, "-1") == 0
			  || strcmp (p_tag->value, "reverse") == 0)
			  oneway = -1;
		  }
		if (params->oneway_strategy != ONEWAY_STRAT_NO_BOTH
		    && params->oneway_strategy != ONEWAY_STRAT_NO_ROUND)
		  {
		      /* testing for junction:roundabout */
		      if (strcmp (p_tag->key, "junction") == 0)
			{
			    if (strcmp (p_tag->value, "roundabout") == 0)
				oneway = 1;
			}
		  }
		if (params->oneway_strategy != ONEWAY_STRAT_NO_BOTH
		    && params->oneway_strategy != ONEWAY_STRAT_NO_MOTOR)
		  {
		      /* testing for highway_motorway or highway_motorway_link */
		      if (strcmp (p_tag->key, "highway") == 0)
			{
			    if (strcmp (p_tag->value, "motorway") == 0
				|| strcmp (p_tag->value, "motorway_link") == 0)
				oneway = 1;
			}
		  }
	    }
      }

    geom = build_linestrings (params, way);
    if (geom)
      {
	  gaiaLinestringPtr ln = geom->FirstLinestring;
	  while (ln)
	    {
		/* inserting any splitted Arc */
		gaiaGeomCollPtr g;
		gaiaLinestringPtr ln2;
		int iv;
		if (gaiaIsClosed (ln))
		  {
		      ln = ln->Next;
		      continue;
		  }
		/* building a new Geometry - simple line */
		g = gaiaAllocGeomColl ();
		g->Srid = 4326;
		ln2 = gaiaAddLinestringToGeomColl (g, ln->Points);
		for (iv = 0; iv < ln->Points; iv++)
		  {
		      /* copying line's points */
		      double x;
		      double y;
		      gaiaGetPoint (ln->Coords, iv, &x, &y);
		      gaiaSetPoint (ln2->Coords, iv, x, y);
		  }
		gaiaToSpatiaLiteBlobWkb (g, &blob, &blob_size);
		if (params->double_arcs)
		    ret =
			arcs_insert_double (params, way->id, class, name,
					    oneway, blob, blob_size);
		else
		    ret =
			arcs_insert (params, way->id, class, name, oneway, blob,
				     blob_size);
		gaiaFreeGeomColl (g);
		if (!ret)
		    return READOSM_ABORT;
		ln = ln->Next;
	    }
	  gaiaFreeGeomColl (geom);
      }
    return READOSM_OK;
}

static int
tmp_nodes_insert (struct aux_params *params, const readosm_node * node)
{
/* inserts a node into the corresponding temporary table */
    int ret;
    if (params->ins_tmp_nodes_stmt == NULL)
	return 1;
    sqlite3_reset (params->ins_tmp_nodes_stmt);
    sqlite3_clear_bindings (params->ins_tmp_nodes_stmt);
    sqlite3_bind_int64 (params->ins_tmp_nodes_stmt, 1, node->id);
    sqlite3_bind_double (params->ins_tmp_nodes_stmt, 2, node->latitude);
    sqlite3_bind_double (params->ins_tmp_nodes_stmt, 3, node->longitude);
    ret = sqlite3_step (params->ins_tmp_nodes_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	return 1;
    fprintf (stderr, "sqlite3_step() error: INS_TMP_NODES\n");
    sqlite3_finalize (params->ins_tmp_nodes_stmt);
    params->ins_tmp_nodes_stmt = NULL;
    return 0;
}

static int
consume_node (const void *user_data, const readosm_node * node)
{
/* processing an OSM Node (ReadOSM callback function) */
    struct aux_params *params = (struct aux_params *) user_data;
    if (!tmp_nodes_insert (params, node))
	return READOSM_ABORT;
    return READOSM_OK;
}

static int
populate_graph_nodes (sqlite3 * handle, const char *table)
{
    int ret;
    char *sql_err = NULL;
    char sql[8192];
    char sql2[1024];
    sqlite3_stmt *query_stmt = NULL;
    sqlite3_stmt *update_stmt = NULL;
/* populating GRAPH_NODES */
    strcpy (sql, "INSERT OR IGNORE INTO graph_nodes (lon, lat) ");
    strcat (sql, "SELECT ST_X(ST_StartPoint(Geometry)), ");
    strcat (sql, "ST_Y(ST_StartPoint(Geometry)) ");
    sprintf (sql2, "FROM \"%s\" ", table);
    strcat (sql, sql2);
    strcat (sql, "UNION ");
    strcat (sql, "SELECT ST_X(ST_EndPoint(Geometry)), ");
    strcat (sql, "ST_Y(ST_EndPoint(Geometry)) ");
    sprintf (sql2, "FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "GraphNodes SQL error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* setting OSM-IDs to graph-nodes */
/* the complete operation is handled as a unique SQL Transaction */
    ret = sqlite3_exec (handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

    strcpy (sql, "SELECT n.ROWID, t.id FROM osm_tmp_nodes AS t ");
    strcat (sql, "JOIN graph_nodes AS n ON (t.lon = n.lon AND t.lat = n.lat)");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto error;
      }

    strcpy (sql, "UPDATE graph_nodes SET osm_id = ? ");
    strcat (sql, "WHERE ROWID = ?");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &update_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto error;
      }

    while (1)
      {
	  ret = sqlite3_step (query_stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		sqlite3_int64 id = sqlite3_column_int64 (query_stmt, 0);
		sqlite3_int64 osm_id = sqlite3_column_int64 (query_stmt, 1);
		/* udating the GraphNote */
		sqlite3_reset (update_stmt);
		sqlite3_clear_bindings (update_stmt);
		sqlite3_bind_int64 (update_stmt, 1, osm_id);
		sqlite3_bind_int64 (update_stmt, 2, id);
		ret = sqlite3_step (update_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (handle));
		      goto error;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		goto error;
	    }
      }

    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    return 1;
  error:
    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
    ret = sqlite3_exec (handle, "ROLLBACK", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    return 0;
}

static int
set_node_ids (sqlite3 * handle, const char *table)
{
/* assigning IDs to Nodes of the Graph */
    int ret;
    char *sql_err = NULL;
    char sql[8192];
    char sql2[1024];
    sqlite3_stmt *query_stmt = NULL;
    sqlite3_stmt *update_stmt = NULL;
/* the complete operation is handled as a unique SQL Transaction */
    ret = sqlite3_exec (handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* querying NodeIds */
    strcpy (sql, "SELECT w.id, n1.ROWID, n2.ROWID ");
    sprintf (sql2, "FROM \"%s\" AS w, ", table);
    strcat (sql, sql2);
    strcat (sql, "graph_nodes AS n1, graph_nodes AS n2 ");
    strcat (sql, "WHERE n1.lon = ST_X(ST_StartPoint(w.Geometry)) ");
    strcat (sql, "AND n1.lat = ST_Y(ST_StartPoint(w.Geometry)) ");
    strcat (sql, "AND n2.lon = ST_X(ST_EndPoint(w.Geometry)) ");
    strcat (sql, "AND n2.lat = ST_Y(ST_EndPoint(w.Geometry))");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto error;
      }

/* updating Arcs */
    sprintf (sql, "UPDATE \"%s\" SET node_from = ?, node_to = ? ", table);
    strcat (sql, "WHERE id = ?");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &update_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto error;
      }

    while (1)
      {
	  ret = sqlite3_step (query_stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		sqlite3_int64 id = sqlite3_column_int64 (query_stmt, 0);
		sqlite3_int64 node_from = sqlite3_column_int64 (query_stmt, 1);
		sqlite3_int64 node_to = sqlite3_column_int64 (query_stmt, 2);
		/* udating the Arc */
		sqlite3_reset (update_stmt);
		sqlite3_clear_bindings (update_stmt);
		sqlite3_bind_int64 (update_stmt, 1, node_from);
		sqlite3_bind_int64 (update_stmt, 2, node_to);
		sqlite3_bind_int64 (update_stmt, 3, id);
		ret = sqlite3_step (update_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (handle));
		      goto error;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		goto error;
	    }
      }

    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    return 1;
  error:
    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
    ret = sqlite3_exec (handle, "ROLLBACK", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    return 0;
}

static double
compute_cost (struct aux_params *params, const char *class, double length)
{
/* computing the travel time [cost] */
    double speed = params->default_speed;	/* speed, in Km/h */
    double msec;
    if (class != NULL)
      {
	  struct aux_speed *ps = params->first_speed;
	  while (ps)
	    {
		if (strcmp (ps->class_name, class) == 0)
		  {
		      speed = ps->speed;
		      break;
		  }
		ps = ps->next;
	    }
      }

    msec = speed * 1000.0 / 3600.0;	/* transforming speed in m/sec */
    return length / msec;
}

static int
set_lengths_costs (struct aux_params *params, const char *table)
{
/* assigning lengths and costs to each Arc of the Graph */
    int ret;
    char *sql_err = NULL;
    char sql[8192];
    char sql2[1024];
    sqlite3_stmt *query_stmt = NULL;
    sqlite3_stmt *update_stmt = NULL;
/* the complete operation is handled as a unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* querying Arcs */
    strcpy (sql, "SELECT id, class, GreatCircleLength(Geometry) ");
    sprintf (sql2, "FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			      &query_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* updating Arcs */
    sprintf (sql, "UPDATE \"%s\" SET length = ?, cost = ? ", table);
    strcat (sql, "WHERE id = ?");
    ret = sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			      &update_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

    while (1)
      {
	  ret = sqlite3_step (query_stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		sqlite3_int64 id = sqlite3_column_int64 (query_stmt, 0);
		const char *class =
		    (const char *) sqlite3_column_text (query_stmt, 1);
		double length = sqlite3_column_double (query_stmt, 2);
		double cost = compute_cost (params, class, length);
		/* udating the Arc */
		sqlite3_reset (update_stmt);
		sqlite3_clear_bindings (update_stmt);
		sqlite3_bind_double (update_stmt, 1, length);
		sqlite3_bind_double (update_stmt, 2, cost);
		sqlite3_bind_int64 (update_stmt, 3, id);
		ret = sqlite3_step (update_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (params->db_handle));
		      goto error;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n",
			sqlite3_errmsg (params->db_handle));
		goto error;
	    }
      }

    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    return 1;
  error:
    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
    ret = sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    return 0;
}

static int
create_qualified_nodes (struct aux_params *params, const char *table)
{
/* creating and feeding the helper table representing Nodes of the Graph */
    int ret;
    char *sql_err = NULL;
    char sql[8192];
    char sql2[1024];
    sqlite3_stmt *query_stmt = NULL;
    sqlite3_stmt *insert_stmt = NULL;
    sqlite3_stmt *update_stmt = NULL;
    char *err_msg = NULL;
    printf ("\nCreating helper table '%s_nodes' ... wait please ...\n", table);
    sprintf (sql, "CREATE TABLE \"%s_nodes\" (\n", table);
    strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "osm_id INTEGER,\n");
    strcat (sql, "cardinality INTEGER NOT NULL)\n");
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE '%s_nodes' error: %s\n", table,
		   err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (params->db_handle);
	  return 0;
      }

    sprintf (sql, "SELECT AddGeometryColumn('%s_nodes', 'geometry', ", table);
    strcat (sql, " 4326, 'POINT', 'XY')");
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "AddGeometryColumn() error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (params->db_handle);
	  return 0;
      }

/* the complete operation is handled as a unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* querying Arcs */
    strcpy (sql, "SELECT node_from, ST_StartPoint(Geometry), ");
    strcat (sql, "node_to, ST_EndPoint(Geometry) ");
    sprintf (sql2, "FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			      &query_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* Inserting Nodes */
    sprintf (sql,
	     "INSERT OR IGNORE INTO \"%s_nodes\" (node_id, cardinality, geometry) ",
	     table);
    strcat (sql, "VALUES (?, 0, ?)");
    ret = sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			      &insert_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

    while (1)
      {
	  ret = sqlite3_step (query_stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		sqlite3_int64 id1 = sqlite3_column_int64 (query_stmt, 0);
		const void *blob1 = sqlite3_column_blob (query_stmt, 1);
		int len1 = sqlite3_column_bytes (query_stmt, 1);
		sqlite3_int64 id2 = sqlite3_column_int64 (query_stmt, 2);
		const void *blob2 = sqlite3_column_blob (query_stmt, 3);
		int len2 = sqlite3_column_bytes (query_stmt, 3);
		/* inserting the Node (from) */
		sqlite3_reset (insert_stmt);
		sqlite3_clear_bindings (insert_stmt);
		sqlite3_bind_int64 (insert_stmt, 1, id1);
		sqlite3_bind_blob (insert_stmt, 2, blob1, len1, SQLITE_STATIC);
		ret = sqlite3_step (insert_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (params->db_handle));
		      goto error;
		  }

		/* inserting the Node (to) */
		sqlite3_reset (insert_stmt);
		sqlite3_clear_bindings (insert_stmt);
		sqlite3_bind_int64 (insert_stmt, 1, id2);
		sqlite3_bind_blob (insert_stmt, 2, blob2, len2, SQLITE_STATIC);
		ret = sqlite3_step (insert_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (params->db_handle));
		      goto error;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n",
			sqlite3_errmsg (params->db_handle));
		goto error;
	    }
      }

    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (insert_stmt != NULL)
	sqlite3_finalize (insert_stmt);
/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
/* re-opening a Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* re-querying Arcs */
    strcpy (sql, "SELECT node_from, node_to ");
    sprintf (sql2, "FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			      &query_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* Updating Nodes */
    sprintf (sql,
	     "UPDATE \"%s_nodes\" SET cardinality = (cardinality + 1) ", table);
    strcat (sql, "WHERE node_id = ?");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &update_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

    while (1)
      {
	  ret = sqlite3_step (query_stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		sqlite3_int64 id1 = sqlite3_column_int64 (query_stmt, 0);
		sqlite3_int64 id2 = sqlite3_column_int64 (query_stmt, 1);
		/* udating the Node (from) */
		sqlite3_reset (update_stmt);
		sqlite3_clear_bindings (update_stmt);
		sqlite3_bind_double (update_stmt, 1, id1);
		ret = sqlite3_step (update_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (params->db_handle));
		      goto error;
		  }

		/* udating the Node (to) */
		sqlite3_reset (update_stmt);
		sqlite3_clear_bindings (update_stmt);
		sqlite3_bind_double (update_stmt, 1, id2);
		ret = sqlite3_step (update_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (params->db_handle));
		      goto error;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n",
			sqlite3_errmsg (params->db_handle));
		goto error;
	    }
      }

    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
/* re-opening a Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* querying Nodes */
    strcpy (sql, "SELECT n.node_id, t.osm_id ");
    sprintf (sql2, "FROM \"%s_nodes\" AS n ", table);
    strcat (sql, sql2);
    strcat (sql, "JOIN graph_nodes AS t ON (");
    strcat (sql, "ST_X(n.Geometry) = t.lon AND ");
    strcat (sql, "ST_Y(n.Geometry) = t.lat)");
    ret = sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			      &query_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* Updating Nodes */
    sprintf (sql, "UPDATE \"%s_nodes\" SET osm_id = ? ", table);
    strcat (sql, "WHERE node_id = ?");
    ret = sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			      &update_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

    while (1)
      {
	  ret = sqlite3_step (query_stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		sqlite3_int64 id = sqlite3_column_int64 (query_stmt, 0);
		sqlite3_int64 osm_id = sqlite3_column_int64 (query_stmt, 1);
		/* udating the Node OSM-ID */
		sqlite3_reset (update_stmt);
		sqlite3_clear_bindings (update_stmt);
		sqlite3_bind_int64 (update_stmt, 1, osm_id);
		sqlite3_bind_int64 (update_stmt, 2, id);
		ret = sqlite3_step (update_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW);
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (params->db_handle));
		      goto error;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n",
			sqlite3_errmsg (params->db_handle));
		goto error;
	    }
      }
/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    printf ("\tHelper table '%s_nodes' successfully created\n", table);
    return 1;
  error:
    if (query_stmt != NULL)
	sqlite3_finalize (query_stmt);
    if (insert_stmt != NULL)
	sqlite3_finalize (insert_stmt);
    if (update_stmt != NULL)
	sqlite3_finalize (update_stmt);
    ret = sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
    return 0;
}

static void
finalize_sql_stmts (struct aux_params *params)
{
/* memory cleanup - prepared statements */
    int ret;
    char *sql_err = NULL;
    if (params->ins_tmp_nodes_stmt != NULL)
	sqlite3_finalize (params->ins_tmp_nodes_stmt);
    if (params->upd_tmp_nodes_stmt != NULL)
	sqlite3_finalize (params->upd_tmp_nodes_stmt);
    if (params->rd_tmp_nodes_stmt != NULL)
	sqlite3_finalize (params->rd_tmp_nodes_stmt);
    if (params->ins_arcs_stmt != NULL)
	sqlite3_finalize (params->ins_arcs_stmt);
/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }
}

static void
create_sql_stmts (struct aux_params *params, int journal_off)
{
/* creating prepared SQL statements */
    sqlite3_stmt *ins_tmp_nodes_stmt;
    sqlite3_stmt *upd_tmp_nodes_stmt;
    sqlite3_stmt *rd_tmp_nodes_stmt;
    sqlite3_stmt *ins_arcs_stmt;
    char sql[1024];
    int ret;
    char *sql_err = NULL;

    if (journal_off)
      {
	  /* disabling the journal: unsafe but faster */
	  ret =
	      sqlite3_exec (params->db_handle, "PRAGMA journal_mode = OFF",
			    NULL, NULL, &sql_err);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "PRAGMA journal_mode=OFF error: %s\n",
			 sql_err);
		sqlite3_free (sql_err);
		return;
	    }
      }

/* the complete operation is handled as a unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }
    strcpy (sql, "INSERT INTO osm_tmp_nodes (id, lat, lon, ref_count) ");
    strcat (sql, "VALUES (?, ?, ?, 0)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_tmp_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "UPDATE osm_tmp_nodes SET ref_count = (ref_count + 1) ");
    strcat (sql, "WHERE id = ?");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &upd_tmp_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "SELECT lon, lat, ref_count FROM osm_tmp_nodes ");
    strcat (sql, "WHERE id = ?");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &rd_tmp_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    if (params->double_arcs == 0)
      {
	  /* bi-directional arcs */
	  sprintf (sql, "INSERT INTO \"%s\" ", params->table);
	  strcat (sql, "(id, osm_id, node_from, node_to, class, ");
	  strcat (sql,
		  "name, oneway_fromto, oneway_tofrom, length, cost, geometry) ");
	  strcat (sql, "VALUES (NULL, ?, -1, -1, ?, ?, ?, ?, -1, -1, ?)");
      }
    else
      {
	  /* unidirectional arcs */
	  sprintf (sql, "INSERT INTO \"%s\" ", params->table);
	  strcat (sql, "(id, osm_id, node_from, node_to, class, ");
	  strcat (sql, "name, length, cost, geometry) ");
	  strcat (sql, "VALUES (NULL, ?, -1, -1, ?, ?, -1, -1, ?)");
      }
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_arcs_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }

    params->ins_tmp_nodes_stmt = ins_tmp_nodes_stmt;
    params->upd_tmp_nodes_stmt = upd_tmp_nodes_stmt;
    params->rd_tmp_nodes_stmt = rd_tmp_nodes_stmt;
    params->ins_arcs_stmt = ins_arcs_stmt;
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
    if (rows < 1);
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

static sqlite3 *
open_db (const char *path, const char *table, int double_arcs, int cache_size,
	 void *cache)
{
/* opening the DB */
    sqlite3 *handle;
    int ret;
    char sql[1024];
    char *err_msg = NULL;
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

    printf ("SQLite version: %s\n", sqlite3_libversion ());
    printf ("SpatiaLite version: %s\n\n", spatialite_version ());
    ret =
	sqlite3_open_v2 (path, &handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", path,
		   sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  return NULL;
      }
    spatialite_init_ex (handle, cache, 0);
    spatialite_autocreate (handle);
    if (cache_size > 0)
      {
	  /* setting the CACHE-SIZE */
	  sprintf (sql, "PRAGMA cache_size=%d", cache_size);
	  sqlite3_exec (handle, sql, NULL, NULL, NULL);
      }

/* checking the GEOMETRY_COLUMNS table */
    strcpy (sql, "PRAGMA table_info(geometry_columns)");
    ret = sqlite3_get_table (handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1);
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
    ret = sqlite3_get_table (handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1);
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
    if (spatialite_gc && spatialite_rs);
    else
	goto unknown;
/* creating the OSM related tables */
    strcpy (sql, "CREATE TABLE osm_tmp_nodes (\n");
    strcat (sql, "id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "lat DOUBLE NOT NULL,\n");
    strcat (sql, "lon DOUBLE NOT NULL,\n");
    strcat (sql, "ref_count INTEGER NOT NULL)\n");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_tmp_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (handle);
	  return NULL;
      }
/* creating the GRAPH temporary nodes */
    strcpy (sql, "CREATE TABLE graph_nodes (\n");
    strcat (sql, "lon DOUBLE NOT NULL,\n");
    strcat (sql, "lat DOUBLE NOT NULL,\n");
    strcat (sql, "osm_id INTEGER,\n");
    strcat (sql, "CONSTRAINT pk_nodes PRIMARY KEY (lon, lat))\n");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'graph_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (handle);
	  return NULL;
      }

    if (double_arcs)
      {
	  /* unidirectional arcs */
	  sprintf (sql, "CREATE TABLE \"%s\" (\n", table);
	  strcat (sql, "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\n");
	  strcat (sql, "osm_id INTEGER NOT NULL,\n");
	  strcat (sql, "class TEXT NOT NULL,\n");
	  strcat (sql, "node_from INTEGER NOT NULL,\n");
	  strcat (sql, "node_to INTEGER NOT NULL,\n");
	  strcat (sql, "name TEXT NOT NULL,\n");
	  strcat (sql, "length DOUBLE NOT NULL,\n"),
	      strcat (sql, "cost DOUBLE NOT NULL)\n");
      }
    else
      {
	  /* bidirectional arcs */
	  sprintf (sql, "CREATE TABLE \"%s\" (\n", table);
	  strcat (sql, "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,\n");
	  strcat (sql, "osm_id INTEGER NOT NULL,\n");
	  strcat (sql, "class TEXT NOT NULL,\n");
	  strcat (sql, "node_from INTEGER NOT NULL,\n");
	  strcat (sql, "node_to INTEGER NOT NULL,\n");
	  strcat (sql, "name TEXT NOT NULL,\n");
	  strcat (sql, "oneway_fromto INTEGER NOT NULL,\n");
	  strcat (sql, "oneway_tofrom INTEGER NOT NULL,\n");
	  strcat (sql, "length DOUBLE NOT NULL,\n"),
	      strcat (sql, "cost DOUBLE NOT NULL)\n");
      }
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE '%s' error: %s\n", table, err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (handle);
	  return NULL;
      }

    sprintf (sql, "SELECT AddGeometryColumn('%s', 'geometry', ", table);
    strcat (sql, " 4326, 'LINESTRING', 'XY')");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "AddGeometryColumn() error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (handle);
	  return NULL;
      }

    return handle;
  unknown:
    if (handle)
	sqlite3_close (handle);
    fprintf (stderr, "DB '%s'\n", path);
    fprintf (stderr, "doesn't seems to contain valid Spatial Metadata ...\n\n");
    fprintf (stderr, "Please, run the 'spatialite-init' SQL script \n");
    fprintf (stderr, "in order to initialize Spatial Metadata\n\n");
    return NULL;
}

static void
db_cleanup (sqlite3 * handle)
{
    int ret;
    char *sql_err = NULL;
/* dropping the OSM_TMP_NODES table */
    printf ("\nDropping temporary table 'osm_tmp_nodes' ... wait please ...\n");
    ret =
	sqlite3_exec (handle, "DROP TABLE osm_tmp_nodes", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "'DROP TABLE osm_tmp_nodes' error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }
    printf ("\tDropped table 'osm_tmp_nodes'\n");
/* dropping the GRAPH_NODES table */
    printf ("\nDropping temporary table 'graph_nodes' ... wait please ...\n");
    ret = sqlite3_exec (handle, "DROP TABLE graph_nodes", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "'DROP TABLE graph_nodes' error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }
    printf ("\tDropped table 'graph_nodes'\n");
}

static void
db_vacuum (sqlite3 * handle)
{
    int ret;
    char *sql_err = NULL;
/* VACUUMing the DB */
    printf ("\nVACUUMing the DB ... wait please ...\n");
    ret = sqlite3_exec (handle, "VACUUM", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "VACUUM error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }
    printf ("\tAll done: OSM graph was successfully loaded\n");
}

static int
parse_kv (char *line, const char **k, const char **v)
{
/* splitting two tokens separated by a colon ':' */
    int i;
    int cnt = 0;
    int pos = -1;
    int len = strlen (line);
    for (i = 0; i < len; i++)
      {
	  if (line[i] == ':')
	    {
		/* delimiter found */
		cnt++;
		pos = i;
	    }
      }
    if (cnt != 1)
      {
	  /* illegal string */
	  return 0;
      }
    line[pos] = '\0';
    *k = line;
    *v = line + pos + 1;
    return 1;
}

static void
free_params (struct aux_params *params)
{
/* memory cleanup - aux params linked lists */
    struct aux_speed *ps;
    struct aux_speed *ps_n;
    struct aux_class *pc;
    struct aux_class *pc_n;
    ps = params->first_speed;
    while (ps)
      {
	  ps_n = ps->next;
	  free (ps->class_name);
	  free (ps);
	  ps = ps_n;
      }
    params->first_speed = NULL;
    params->last_speed = NULL;
    pc = params->first_include;
    while (pc)
      {
	  pc_n = pc->next;
	  free (pc->class_name);
	  if (pc->sub_class != NULL)
	      free (pc->sub_class);
	  free (pc);
	  pc = pc_n;
      }
    params->first_include = NULL;
    params->last_include = NULL;
    pc = params->first_ignore;
    while (pc)
      {
	  pc_n = pc->next;
	  free (pc->class_name);
	  if (pc->sub_class != NULL)
	      free (pc->sub_class);
	  free (pc);
	  pc = pc_n;
      }
    params->first_ignore = NULL;
    params->last_ignore = NULL;
}

static void
add_speed_class (struct aux_params *params, const char *class, double speed)
{
/* inserting a class speed into the linked list */
    int len = strlen (class);
    struct aux_speed *p = malloc (sizeof (struct aux_speed));
    p->class_name = malloc (len + 1);
    strcpy (p->class_name, class);
    p->speed = speed;
    p->next = NULL;
    if (params->first_speed == NULL)
	params->first_speed = p;
    if (params->last_speed != NULL)
	params->last_speed->next = p;
    params->last_speed = p;
}

static void
add_include_class (struct aux_params *params, const char *class,
		   const char *sub_class)
{
/* inserting an Include class into the linked list */
    int len1 = strlen (class);
    int len2 = strlen (sub_class);
    struct aux_class *p = malloc (sizeof (struct aux_class));
    p->class_name = malloc (len1 + 1);
    strcpy (p->class_name, class);
    if (len2 == 0)
	p->sub_class = NULL;
    else
      {
	  p->sub_class = malloc (len2 + 1);
	  strcpy (p->sub_class, sub_class);
      }
    p->next = NULL;
    if (params->first_include == NULL)
	params->first_include = p;
    if (params->last_include != NULL)
	params->last_include->next = p;
    params->last_include = p;
}

static void
add_ignore_class (struct aux_params *params, const char *class,
		  const char *sub_class)
{
/* inserting an Ignore class into the linked list */
    int len1 = strlen (class);
    int len2 = strlen (sub_class);
    struct aux_class *p = malloc (sizeof (struct aux_class));
    p->class_name = malloc (len1 + 1);
    strcpy (p->class_name, class);
    p->sub_class = malloc (len2 + 1);
    strcpy (p->sub_class, sub_class);
    p->next = NULL;
    if (params->first_ignore == NULL)
	params->first_ignore = p;
    if (params->last_ignore != NULL)
	params->last_ignore->next = p;
    params->last_ignore = p;
}

static int
parse_template_line (struct aux_params *params, const char *line)
{
/* parsing a template line */
    char clean[8192];
    char *out = clean;
    const char *in = line;
    int i;
    *out = '\0';
    while (1)
      {
	  if (*in == '\0' || *in == '#')
	    {
		*out = '\0';
		break;
	    }
	  *out++ = *in++;
      }
    for (i = (int) strlen (clean) - 1; i >= 0; i--)
      {
	  /* cleaning any tralining space/tab */
	  if (clean[i] == ' ' || clean[i] == '\t')
	      clean[i] = '\0';
	  else
	      break;
      }
    if (*clean == '\0')
	return 1;		/* ignoring empty lines */
    if (strncmp (clean, "NodingStrategy:", 15) == 0)
      {
	  if (strcmp (clean + 15, "way-ends") == 0)
	    {
		params->noding_strategy = NODE_STRAT_ENDS;
		return 1;
	    }
	  else if (strcmp (clean + 15, "none") == 0)
	    {
		params->noding_strategy = NODE_STRAT_NONE;
		return 1;
	    }
	  else if (strcmp (clean + 15, "all") == 0)
	    {
		params->noding_strategy = NODE_STRAT_ALL;
		return 1;
	    }
	  else
	      return 0;
      }
    else if (strncmp (clean, "OnewayStrategy:", 15) == 0)
      {
	  if (strcmp (clean + 15, "full") == 0)
	    {
		params->oneway_strategy = ONEWAY_STRAT_FULL;
		return 1;
	    }
	  else if (strcmp (clean + 15, "none") == 0)
	    {
		params->oneway_strategy = ONEWAY_STRAT_NONE;
		return 1;
	    }
	  else if (strcmp (clean + 15, "ignore-roundabout") == 0)
	    {
		params->oneway_strategy = ONEWAY_STRAT_NO_ROUND;
		return 1;
	    }
	  else if (strcmp (clean + 15, "ignore-motorway") == 0)
	    {
		params->oneway_strategy = ONEWAY_STRAT_NO_MOTOR;
		return 1;
	    }
	  else if (strcmp (clean + 15, "ignore-both-roundabout-and-motorway") ==
		   0)
	    {
		params->oneway_strategy = ONEWAY_STRAT_NO_BOTH;
		return 1;
	    }
	  else
	      return 0;
      }
    else if (strncmp (clean, "ClassInclude:", 13) == 0)
      {
	  const char *k;
	  const char *v;
	  if (parse_kv (clean + 13, &k, &v))
	    {
		if (strlen (k) > 0)
		    add_include_class (params, k, v);
		return 1;
	    }
	  else
	      return 0;
      }
    else if (strncmp (clean, "ClassIgnore:", 12) == 0)
      {
	  const char *k;
	  const char *v;
	  if (parse_kv (clean + 12, &k, &v))
	    {
		if (strlen (k) > 0 && strlen (v) > 0)
		    add_ignore_class (params, k, v);
		return 1;
	    }
	  else
	      return 0;
      }
    else if (strncmp (clean, "SpeedClass:", 11) == 0)
      {
	  const char *k;
	  const char *v;
	  if (parse_kv (clean + 11, &k, &v))
	    {
		if (*k == '\0')
		    params->default_speed = atof (v);
		else
		    add_speed_class (params, k, atof (v));
		return 1;
	    }
	  else
	      return 0;
      }
/* some illegal expression found */
    return 0;
}

static int
parse_template (struct aux_params *params, const char *template_path)
{
/* parsing a template-file */
    char line[8192];
    char *p = line;
    int c;
    int lineno = 0;
    FILE *in = fopen (template_path, "rb");
    if (in == NULL)
      {
	  fprintf (stderr, "Unable to open template-file \"%s\"\n",
		   template_path);
	  return 0;
      }

    while ((c = getc (in)) != EOF)
      {

	  if (c == '\r')
	      continue;
	  if (c == '\n')
	    {
		*p = '\0';
		lineno++;
		if (!parse_template_line (params, line))
		  {
		      fprintf (stderr,
			       "Template-file \"%s\"\nParsing error on line %d\n\n",
			       template_path, lineno);
		      free_params (params);
		      return 0;
		  }
		p = line;
		continue;
	    }
	  *p++ = c;
      }

    fclose (in);
    return 1;
}

static int
print_template (const char *template_path, int railways)
{
/* printing a default template-file */
    FILE *out = fopen (template_path, "w");
    if (out == NULL)
      {
	  fprintf (stderr, "Unable to create template-file \"%s\"\n",
		   template_path);
	  return 0;
      }

    fprintf (out,
	     "###############################################################\n");
    fprintf (out, "#\n");
    fprintf (out, "# the '#' char represents a comment marker:\n");
    fprintf (out, "# any text until the next new-line (NL) char will\n");
    fprintf (out, "# be ignored at all.\n");
    fprintf (out, "#\n\n");
    fprintf (out,
	     "###############################################################\n");
    fprintf (out, "#\n");
    fprintf (out, "# NodingStrategy section\n");
    fprintf (out, "#\n");
    fprintf (out, "# - NodingStrategy:way-ends\n");
    fprintf (out,
	     "#   any Way end-point (both extremities) is assumed to represent\n");
    fprintf (out, "#   a Node into the Graph [network] to be built.\n");
    fprintf (out, "# - NodingStrategy:none\n");
    fprintf (out,
	     "#   any Way is assumed to directly represent an Arc into the Graph\n");
    fprintf (out,
	     "#   [network] to be built. No attempt to split and renode the\n");
    fprintf (out, "#   Graph's Arcs will be performed.\n");
    fprintf (out, "# - NodingStrategy:all\n");
    fprintf (out,
	     "#   any Way point is assumed to represent a Node into the Graph\n");
    fprintf (out,
	     "#   [network] to be built, if it's shared by two or more Ways.\n");
    fprintf (out, "#\n\n");
    if (railways)
      {
	  fprintf (out, "NodingStrategy:all # default value for Railway\n");
	  fprintf (out, "# NodingStrategy:none\n");
	  fprintf (out, "# NodingStrategy:way-ends\n\n\n");
      }
    else
      {
	  fprintf (out, "NodingStrategy:way-ends # default value\n");
	  fprintf (out, "# NodingStrategy:none\n");
	  fprintf (out, "# NodingStrategy:all\n\n\n");
      }

    fprintf (out,
	     "###############################################################\n");
    fprintf (out, "#\n");
    fprintf (out, "# OnewayStrategy section\n");
    fprintf (out, "#\n");
    fprintf (out, "# - OnewayStrategy:full\n");
    fprintf (out,
	     "#   the following OSM tags will be assumed to identify oneways:\n");
    fprintf (out,
	     "#   * oneway:1, oneway:true or oneway:yes [oneway, normal direction]\n");
    fprintf (out,
	     "#   * oneway:-1 or oneway:reverse [oneway, reverse direction]\n");
    fprintf (out,
	     "#   * junction:roundabout, highway:motorway or highway:motorway_link\n");
    fprintf (out, "#   * [implicit oneway, normal direction]\n");
    fprintf (out, "# - OnewayStrategy:none\n");
    fprintf (out,
	     "#   all Arcs will be assumed to be bidirectional (no oneway at all).\n");
    fprintf (out, "# - OnewayStrategy:ignore-roundabout\n");
    fprintf (out,
	     "#   any junction:roundabout tag will not be assumed to mark an oneway.\n");
    fprintf (out, "# - OnewayStrategy:ignore-motorway\n");
    fprintf (out,
	     "#   any highway:motorway or highway:motorway_link tag will not be \n");
    fprintf (out, "#   assumed to mark an oneway.\n");
    fprintf (out, "# - OnewayStrategy:ignore-both-roundabout-and-motorway\n");
    fprintf (out,
	     "#   any junction:roundabout, highway:motorway or highway:motorway_link\n");
    fprintf (out, "#   tag will not be assumed to mark an oneway.\n");
    fprintf (out, "#\n\n");
    if (railways)
      {
	  fprintf (out, "OnewayStrategy:none # default value for Railways\n");
	  fprintf (out, "# OnewayStrategy:full\n");
	  fprintf (out, "# OnewayStrategy:ignore-roundabout\n");
	  fprintf (out, "# OnewayStrategy:ignore-motorway\n");
	  fprintf (out,
		   "# OnewayStrategy:ignore-both-roundabout-and-motorway\n\n\n");
      }
    else
      {
	  fprintf (out, "OnewayStrategy:full # default value\n");
	  fprintf (out, "# OnewayStrategy:none\n");
	  fprintf (out, "# OnewayStrategy:ignore-roundabout\n");
	  fprintf (out, "# OnewayStrategy:ignore-motorway\n");
	  fprintf (out,
		   "# OnewayStrategy:ignore-both-roundabout-and-motorway\n\n\n");
      }

    fprintf (out,
	     "###############################################################\n");
    fprintf (out, "#\n");
    fprintf (out, "# ClassInclude section\n");
    fprintf (out, "#\n");
    fprintf (out, "# - tokens are delimited by colons ':'\n");
    fprintf (out,
	     "# - the second and third tokens represents a Class-name tag\n");
    fprintf (out,
	     "#   identifying the Arcs of the Graph: i.e. any Way exposing\n");
    fprintf (out, "#   this tag will be processed.\n");
    fprintf (out,
	     "# - special case: suppressing the third token selects any\n");
    fprintf (out, "#   generic main-class tag to be processed\n");
    fprintf (out, "#\n\n");
    if (railways)
	fprintf (out,
		 "ClassInclude:railway:rail # default value for Railways\n\n\n");
    else
	fprintf (out,
		 "ClassInclude:highway: # default value (all kind of highway)\n\n\n");
    fprintf (out,
	     "###############################################################\n");
    fprintf (out, "#\n");
    fprintf (out, "# ClassIgnore section\n");
    fprintf (out, "#\n");
    fprintf (out, "# - tokens are delimited by colons ':'\n");
    fprintf (out,
	     "# - the second and third tokens represents a Class-name tag\n");
    fprintf (out, "#   identifying Ways to be completely ignored.\n");
    fprintf (out, "#\n\n");
    if (railways)
	fprintf (out, "# none for Railways\n\n\n");
    else
      {
	  fprintf (out, "ClassIgnore:highway:pedestrian\n");
	  fprintf (out, "ClassIgnore:highway:track\n");
	  fprintf (out, "ClassIgnore:highway:services\n");
	  fprintf (out, "ClassIgnore:highway:bus_guideway\n");
	  fprintf (out, "ClassIgnore:highway:path\n");
	  fprintf (out, "ClassIgnore:highway:cycleway\n");
	  fprintf (out, "ClassIgnore:highway:footway\n");
	  fprintf (out, "ClassIgnore:highway:byway\n");
	  fprintf (out, "ClassIgnore:highway:steps\n\n\n");
      }

    fprintf (out,
	     "###############################################################\n");
    fprintf (out, "#\n");
    fprintf (out, "# SpeedClass section\n");
    fprintf (out, "#\n");
    fprintf (out, "# - tokens are delimited by colons ':'\n");
    fprintf (out, "# - the second token represents the Road Class-name\n");
    fprintf (out, "#   [no name, i.e. '::' identifies the defaul value\n");
    fprintf (out, "#   to be applied when no specific class match is found]\n");
    fprintf (out, "# - the third token represents the corresponding speed\n");
    fprintf (out, "#   [expressed in Km/h]\n");
    fprintf (out, "#\n\n");
    if (railways)
	fprintf (out, "SpeedClass:rail:60.0\n\n\n");
    else
      {
	  fprintf (out, "SpeedClass::30.0 # default value\n");
	  fprintf (out, "SpeedClass:motorway:110.0\n");
	  fprintf (out, "SpeedClass:trunk:110.0\n");
	  fprintf (out, "SpeedClass:primary:90.0\n");
	  fprintf (out, "SpeedClass:secondary:70.0\n");
	  fprintf (out, "SpeedClass:tertiary:50.0\n");
	  fprintf (out, "# SpeedClass:yet_anotherclass_1:1.0\n");
	  fprintf (out, "# SpeedClass:yet_anotherclass_2:2.0\n");
	  fprintf (out, "# SpeedClass:yet_anotherclass_3:3.0\n\n\n");
      }

    fclose (out);
    return 1;
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_osm_net ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr, "-o or --osm-path pathname       the OSM-XML file path\n");
    fprintf (stderr,
	     "                 both OSM-XML (*.osm) and OSM-ProtoBuf\n");
    fprintf (stderr,
	     "                 (*.osm.pbf) are indifferently supported.\n\n");
    fprintf (stderr,
	     "-d or --db-path  pathname       the SpatiaLite DB path\n");
    fprintf (stderr,
	     "-T or --table    table_name     the db table to be feeded\n\n");
    fprintf (stderr, "you can specify the following options as well\n");
    fprintf (stderr,
	     "-cs or --cache-size    num      DB cache size (how many pages)\n");
    fprintf (stderr,
	     "-m or --in-memory               using IN-MEMORY database\n");
    fprintf (stderr,
	     "-jo or --journal-off            unsafe [but faster] mode\n");
    fprintf (stderr, "-2 or --undirectional           double arcs\n\n");
    fprintf (stderr,
	     "--roads                         extract roads [default]\n");
    fprintf (stderr, "--railways                      extract railways\n");
    fprintf (stderr,
	     "                                [mutually exclusive]\n\n");
    fprintf (stderr, "template-file specific options:\n");
    fprintf (stderr,
	     "-ot or --out-template  path     creates a default template-file\n");
    fprintf (stderr,
	     "-tf or --template-file path     using a template-file\n\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    int i;
    int next_arg = ARG_NONE;
    const char *osm_path = NULL;
    const char *db_path = NULL;
    const char *table = NULL;
    int in_memory = 0;
    int cache_size = 0;
    int journal_off = 0;
    int double_arcs = 0;
    int railways = 0;
    int out_template = 0;
    int use_template = 0;
    const char *template_path = NULL;
    int error = 0;
    sqlite3 *handle;
    struct aux_params params;
    const void *osm_handle;
    void *cache;

/* initializing the aux-struct */
    params.db_handle = NULL;
    params.ins_tmp_nodes_stmt = NULL;
    params.upd_tmp_nodes_stmt = NULL;
    params.rd_tmp_nodes_stmt = NULL;
    params.ins_arcs_stmt = NULL;
    params.noding_strategy = NODE_STRAT_ENDS;
    params.oneway_strategy = ONEWAY_STRAT_FULL;
    params.default_speed = 30.0;
    params.first_speed = NULL;
    params.last_speed = NULL;
    params.first_include = NULL;
    params.last_include = NULL;
    params.first_ignore = NULL;
    params.last_ignore = NULL;
    for (i = 1; i < argc; i++)
      {
	  /* parsing the invocation arguments */
	  if (next_arg != ARG_NONE)
	    {
		switch (next_arg)
		  {
		  case ARG_OSM_PATH:
		      osm_path = argv[i];
		      break;
		  case ARG_DB_PATH:
		      db_path = argv[i];
		      break;
		  case ARG_TABLE:
		      table = argv[i];
		      break;
		  case ARG_TEMPLATE_PATH:
		      template_path = argv[i];
		      break;
		  case ARG_CACHE_SIZE:
		      cache_size = atoi (argv[i]);
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
	  if (strcmp (argv[i], "-o") == 0)
	    {
		next_arg = ARG_OSM_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--osm-path") == 0)
	    {
		next_arg = ARG_OSM_PATH;
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
	  if (strcmp (argv[i], "-T") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--table") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--cache-size") == 0
	      || strcmp (argv[i], "-cs") == 0)
	    {
		next_arg = ARG_CACHE_SIZE;
		continue;
	    }
	  if (strcmp (argv[i], "-ot") == 0)
	    {
		out_template = 1;
		use_template = 0;
		next_arg = ARG_TEMPLATE_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--out-template") == 0)
	    {
		out_template = 1;
		use_template = 0;
		next_arg = ARG_TEMPLATE_PATH;
		continue;
	    }
	  if (strcmp (argv[i], "-tf") == 0)
	    {
		out_template = 0;
		use_template = 1;
		next_arg = ARG_TEMPLATE_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--template-file") == 0)
	    {
		out_template = 0;
		use_template = 1;
		next_arg = ARG_TEMPLATE_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "-m") == 0)
	    {
		in_memory = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--in-memory") == 0)
	    {
		in_memory = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "-jo") == 0)
	    {
		journal_off = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--journal-off") == 0)
	    {
		journal_off = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "-2") == 0)
	    {
		double_arcs = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--unidirectional") == 0)
	    {
		double_arcs = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--roads") == 0)
	    {
		railways = 0;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--railways") == 0)
	    {
		railways = 1;
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
    if (out_template)
      {
	  /* if out-template is set this one the unique option to be honored */
	  if (!template_path)
	    {
		fprintf (stderr,
			 "did you forget setting the --out-template path argument ?\n");
		error = 1;
	    }
	  if (print_template (template_path, railways))
	      printf ("template-file \"%s\" successfully created\n\n",
		      template_path);
	  return 0;
      }
    if (!osm_path)
      {
	  fprintf (stderr,
		   "did you forget setting the --osm-path argument ?\n");
	  error = 1;
      }
    if (!db_path)
      {
	  fprintf (stderr, "did you forget setting the --db-path argument ?\n");
	  error = 1;
      }
    if (!table)
      {
	  fprintf (stderr, "did you forget setting the --table argument ?\n");
	  error = 1;
      }
    if (railways)
      {
	  /* Railways default settings */
	  params.noding_strategy = NODE_STRAT_ALL;
	  params.oneway_strategy = ONEWAY_STRAT_NONE;
	  params.default_speed = 60.0;
      }
    if (use_template)
      {
	  /* use-template is set */
	  if (!template_path)
	    {
		fprintf (stderr,
			 "did you forget setting the --template-file path argument ?\n");
		error = 1;
	    }
	  if (parse_template (&params, template_path))
	      printf ("template-file \"%s\" successfully acquired\n\n",
		      template_path);
	  else
	      return -1;
      }
    if (error)
      {
	  do_help ();
	  free_params (&params);
	  return -1;
      }

/* opening the DB */
    if (in_memory)
	cache_size = 0;
    cache = spatialite_alloc_connection ();
    handle = open_db (db_path, table, double_arcs, cache_size, cache);
    if (!handle)
      {
	  free_params (&params);
	  return -1;
      }
    if (in_memory)
      {
	  /* loading the DB in-memory */
	  sqlite3 *mem_handle;
	  sqlite3_backup *backup;
	  int ret;
	  ret =
	      sqlite3_open_v2 (":memory:", &mem_handle,
			       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
			       NULL);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "cannot open 'MEMORY-DB': %s\n",
			 sqlite3_errmsg (mem_handle));
		sqlite3_close (mem_handle);
		free_params (&params);
		return -1;
	    }
	  backup = sqlite3_backup_init (mem_handle, "main", handle, "main");
	  if (!backup)
	    {
		fprintf (stderr, "cannot load 'MEMORY-DB'\n");
		sqlite3_close (handle);
		sqlite3_close (mem_handle);
		free_params (&params);
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
	  handle = mem_handle;
	  printf ("\nusing IN-MEMORY database\n");
	  spatialite_cleanup_ex (cache);
	  cache = spatialite_alloc_connection ();
	  spatialite_init_ex (handle, cache, 0);
      }
    params.db_handle = handle;
    params.table = table;
    params.double_arcs = double_arcs;
    if (use_template == 0)
      {
	  /* not using template: setting default params */
	  if (railways == 1)
	      add_include_class (&params, "railway", "rail");
	  else
	      add_include_class (&params, "highway", "");
	  if (railways == 0)
	    {
		/* setting default Road Ignore classes */
		add_ignore_class (&params, "highway", "pedestrian");
		add_ignore_class (&params, "highway", "track");
		add_ignore_class (&params, "highway", "services");
		add_ignore_class (&params, "highway", "bus_guideway");
		add_ignore_class (&params, "highway", "path");
		add_ignore_class (&params, "highway", "cycleway");
		add_ignore_class (&params, "highway", "footway");
		add_ignore_class (&params, "highway", "bridleway");
		add_ignore_class (&params, "highway", "byway");
		add_ignore_class (&params, "highway", "steps");
	    }
	  if (railways == 0)
	    {
		/* setting default Road Speeds */
		add_speed_class (&params, "motorway", 110.0);
		add_speed_class (&params, "trunk", 110.0);
		add_speed_class (&params, "primary", 90.0);
		add_speed_class (&params, "secondary", 70.0);
		add_speed_class (&params, "tertiary", 50.0);
	    }
      }

/* creating SQL prepared statements */
    create_sql_stmts (&params, journal_off);
    printf ("\nParsing input: Pass 1 [Nodes and Ways] ...\n");
/* parsing the input OSM-file [Pass 1] */
    if (readosm_open (osm_path, &osm_handle) != READOSM_OK)
      {
	  fprintf (stderr, "cannot open %s\n", osm_path);
	  finalize_sql_stmts (&params);
	  sqlite3_close (handle);
	  readosm_close (osm_handle);
	  free_params (&params);
	  return -1;
      }
    if (readosm_parse
	(osm_handle, &params, consume_node, consume_way_1, NULL) != READOSM_OK)
      {
	  fprintf (stderr, "unrecoverable error while parsing %s\n", osm_path);
	  finalize_sql_stmts (&params);
	  sqlite3_close (handle);
	  readosm_close (osm_handle);
	  free_params (&params);
	  return -1;
      }
    readosm_close (osm_handle);
    printf ("Parsing input: Pass 2 [Arcs of the Graph] ...\n");
/* parsing the input OSM-file [Pass 2] */
    if (readosm_open (osm_path, &osm_handle) != READOSM_OK)
      {
	  fprintf (stderr, "cannot open %s\n", osm_path);
	  finalize_sql_stmts (&params);
	  sqlite3_close (handle);
	  readosm_close (osm_handle);
	  free_params (&params);
	  return -1;
      }
    if (readosm_parse (osm_handle, &params, NULL, consume_way_2, NULL) !=
	READOSM_OK)
      {
	  fprintf (stderr, "unrecoverable error while parsing %s\n", osm_path);
	  finalize_sql_stmts (&params);
	  sqlite3_close (handle);
	  readosm_close (osm_handle);
	  free_params (&params);
	  return -1;
      }
    readosm_close (osm_handle);
/* finalizing SQL prepared statements */
    finalize_sql_stmts (&params);
/* populating the GRAPH_NODES table */
    if (!populate_graph_nodes (handle, table))
      {
	  fprintf (stderr,
		   "unrecoverable error while extracting GRAPH_NODES\n");
	  sqlite3_close (handle);
	  goto quit;
      }

/* assigning NodeIds to Arcs */
    if (!set_node_ids (handle, table))
      {
	  fprintf (stderr,
		   "unrecoverable error while assignign NODE-IDs to Arcs\n");
	  sqlite3_close (handle);
	  goto quit;
      }

/* computing Length and Cost for each Arc */
    if (!set_lengths_costs (&params, table))
      {
	  fprintf (stderr,
		   "unrecoverable error while assignign Length and Cost to Arcs\n");
	  sqlite3_close (handle);
	  goto quit;
      }

/* extracting qualified Nodes */
    if (!create_qualified_nodes (&params, table))
      {
	  fprintf (stderr,
		   "unrecoverable error while extracting qualified Nodes\n");
	  sqlite3_close (handle);
	  goto quit;
      }

  quit:
/* dropping the temporary tables */
    db_cleanup (handle);
    if (in_memory)
      {
	  /* exporting the in-memory DB to filesystem */
	  sqlite3 *disk_handle;
	  sqlite3_backup *backup;
	  int ret;
	  printf ("\nexporting IN_MEMORY database ... wait please ...\n");
	  ret =
	      sqlite3_open_v2 (db_path, &disk_handle,
			       SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
			       NULL);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "cannot open '%s': %s\n", db_path,
			 sqlite3_errmsg (disk_handle));
		sqlite3_close (disk_handle);
		free_params (&params);
		return -1;
	    }
	  backup = sqlite3_backup_init (disk_handle, "main", handle, "main");
	  if (!backup)
	    {
		fprintf (stderr, "Backup failure: 'MEMORY-DB' wasn't saved\n");
		sqlite3_close (handle);
		sqlite3_close (disk_handle);
		free_params (&params);
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
	  handle = disk_handle;
	  printf ("\tIN_MEMORY database successfully exported\n");
      }
/* VACUUMing */
    db_vacuum (handle);
    sqlite3_close (handle);
    spatialite_cleanup_ex (cache);
    free_params (&params);
    spatialite_shutdown ();
    return 0;
}
