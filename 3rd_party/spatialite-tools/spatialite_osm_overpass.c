/* 
/ spatialite_osm_overpass
/
/ a tool downloading OSM datasets via the Overpass wep API
/
/ version 1.0, 2014 November 13
/
/ Author: Sandro Furieri a.furieri@lqt.it
/
/ Copyright (C) 2014  Alessandro Furieri
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

#include <libxml/parser.h>
#include <libxml/nanohttp.h>

#include <sqlite3.h>
#include <spatialite/gaiageo.h>
#include <spatialite.h>

#define ARG_NONE		0
#define ARG_OSM_URL		1
#define ARG_MINX		2
#define ARG_MINY		3
#define ARG_MAXX		4
#define ARG_MAXY		5
#define ARG_DB_PATH		6
#define ARG_MODE		7
#define ARG_CACHE_SIZE	8

#define MODE_RAW	1
#define MODE_MAP	2
#define MODE_RAIL	3
#define MODE_ROAD	4

#define OBJ_NODES		1
#define OBJ_WAYS		2
#define OBJ_RELATIONS	3

#if defined(_WIN32)
#define atol_64		_atoi64
#else
#define atol_64		atoll
#endif

struct layers
{
    const char *name;
    int ok_point;
    int ok_linestring;
    int ok_polygon;
    int ok_multi_linestring;
    int ok_multi_polygon;
    sqlite3_stmt *ins_point_stmt;
    sqlite3_stmt *ins_linestring_stmt;
    sqlite3_stmt *ins_polygon_stmt;
    sqlite3_stmt *ins_multi_linestring_stmt;
    sqlite3_stmt *ins_multi_polygon_stmt;
} base_layers[] =
{
    {
    "highway", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "junction", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "traffic_calming", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "traffic_sign", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "service", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "barrier", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "cycleway", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "tracktype", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "waterway", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "railway", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "aeroway", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "aerialway", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "power", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "man_made", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "leisure", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "amenity", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "shop", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "tourism", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "historic", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "landuse", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "military", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "natural", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "geological", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "route", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "boundary", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "sport", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "abutters", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "accessories", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "properties", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "restrictions", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "place", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "building", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
    "parking", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},
    {
NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL},};

struct download_tile
{
/* an helper struct corresponging to a single download tile */
    int tile_no;
    double minx;
    double miny;
    double maxx;
    double maxy;
    struct download_tile *next;
};

struct tiled_download
{
/* a tiled downloader object */
    int count;
    struct download_tile *first;
    struct download_tile *last;
};

struct aux_params
{
/* an auxiliary struct used for XML parsing */
    sqlite3 *db_handle;
    void *cache;
    sqlite3_stmt *ins_nodes_stmt;
    sqlite3_stmt *ins_node_tags_stmt;
    sqlite3_stmt *ins_ways_stmt;
    sqlite3_stmt *ins_way_tags_stmt;
    sqlite3_stmt *ins_way_refs_stmt;
    sqlite3_stmt *ins_relations_stmt;
    sqlite3_stmt *ins_relation_tags_stmt;
    sqlite3_stmt *ins_relation_refs_stmt;
    int wr_nodes;
    int wr_node_tags;
    int wr_ways;
    int wr_way_tags;
    int wr_way_refs;
    int wr_relations;
    int wr_rel_tags;
    int wr_rel_refs;
    const char *osm_url;
    int mode;
    sqlite3_int64 current_node_id;
    int current_node_tag_sub;
    sqlite3_int64 current_way_id;
    int current_way_tag_sub;
    int current_way_ref_sub;
    sqlite3_int64 current_rel_id;
    int current_rel_tag_sub;
    int current_rel_ref_sub;
};

struct aux_arc
{
/* an helper struct used to build Road/Rail arcs */
    sqlite3_int64 node_from;
    sqlite3_int64 node_to;
    gaiaGeomCollPtr geom_from;
    gaiaGeomCollPtr geom_to;
    gaiaGeomCollPtr geom;
    struct aux_arc *next;
};

struct aux_arc_container
{
/* container for Road/Rail arcs */
    struct aux_arc *first;
    struct aux_arc *last;
};

static void
finalize_map_stmts ()
{
/* finalizing all Map statements */
    struct layers *layer;
    int i = 0;

    while (1)
      {
	  layer = &(base_layers[i++]);
	  if (layer->name == NULL)
	      break;
	  if (layer->ins_point_stmt != NULL)
	      sqlite3_finalize (layer->ins_point_stmt);
	  if (layer->ins_linestring_stmt != NULL)
	      sqlite3_finalize (layer->ins_linestring_stmt);
	  if (layer->ins_polygon_stmt != NULL)
	      sqlite3_finalize (layer->ins_polygon_stmt);
	  if (layer->ins_multi_linestring_stmt != NULL)
	      sqlite3_finalize (layer->ins_multi_linestring_stmt);
	  if (layer->ins_multi_polygon_stmt != NULL)
	      sqlite3_finalize (layer->ins_multi_polygon_stmt);
      }
}

static void
add_download_tile (struct tiled_download *obj, double minx, double miny,
		   double maxx, double maxy)
{
/* inserting a further tile into the downloader object */
    struct download_tile *tile = malloc (sizeof (struct download_tile));
    tile->tile_no = obj->count;
    tile->minx = minx;
    tile->miny = miny;
    tile->maxx = maxx;
    tile->maxy = maxy;
    tile->next = NULL;
    if (obj->first == NULL)
	obj->first = tile;
    if (obj->last != NULL)
	obj->last->next = tile;
    obj->last = tile;
    obj->count++;
}

static void
downloader_cleanup (struct tiled_download *obj)
{
/* freeing the tiled downloader object */
    struct download_tile *tile;
    struct download_tile *tile_n;
    tile = obj->first;
    while (tile != NULL)
      {
	  tile_n = tile->next;
	  free (tile);
	  tile = tile_n;
      }
}

static int
insert_node_tag (struct aux_params *params, const char *k, const char *v)
{
/* inserting a raw <node><tag> into the DBMS */
    int ret;
    sqlite3_reset (params->ins_node_tags_stmt);
    sqlite3_clear_bindings (params->ins_node_tags_stmt);
    sqlite3_bind_int64 (params->ins_node_tags_stmt, 1, params->current_node_id);
    sqlite3_bind_int (params->ins_node_tags_stmt, 2,
		      params->current_node_tag_sub);
    sqlite3_bind_text (params->ins_node_tags_stmt, 3, k, strlen (k),
		       SQLITE_STATIC);
    sqlite3_bind_text (params->ins_node_tags_stmt, 4, v, strlen (v),
		       SQLITE_STATIC);
    ret = sqlite3_step (params->ins_node_tags_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_node_tags += 1;
    params->current_node_tag_sub += 1;
    return 1;
}

static int
insert_way_ref (struct aux_params *params, sqlite3_int64 node_id)
{
/* inserting a raw <way><nd> into the DBMS */
    int ret;
    sqlite3_reset (params->ins_way_refs_stmt);
    sqlite3_clear_bindings (params->ins_way_refs_stmt);
    sqlite3_bind_int64 (params->ins_way_refs_stmt, 1, params->current_way_id);
    sqlite3_bind_int (params->ins_way_refs_stmt, 2,
		      params->current_way_ref_sub);
    sqlite3_bind_int64 (params->ins_way_refs_stmt, 3, node_id);
    ret = sqlite3_step (params->ins_way_refs_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_way_refs += 1;
    params->current_way_ref_sub += 1;
    return 1;
}

static int
insert_way_tag (struct aux_params *params, const char *k, const char *v)
{
/* inserting a raw <way><tag> into the DBMS */
    int ret;
    sqlite3_reset (params->ins_way_tags_stmt);
    sqlite3_clear_bindings (params->ins_way_tags_stmt);
    sqlite3_bind_int64 (params->ins_way_tags_stmt, 1, params->current_way_id);
    sqlite3_bind_int (params->ins_way_tags_stmt, 2,
		      params->current_way_tag_sub);
    sqlite3_bind_text (params->ins_way_tags_stmt, 3, k, strlen (k),
		       SQLITE_STATIC);
    sqlite3_bind_text (params->ins_way_tags_stmt, 4, v, strlen (v),
		       SQLITE_STATIC);
    ret = sqlite3_step (params->ins_way_tags_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_way_tags += 1;
    params->current_way_tag_sub += 1;
    return 1;
}

static int
insert_relation_ref (struct aux_params *params, const char *type,
		     sqlite3_int64 ref, const char *role)
{
/* inserting a raw <relation><member> into the DBMS */
    int ret;
    sqlite3_reset (params->ins_relation_refs_stmt);
    sqlite3_clear_bindings (params->ins_relation_refs_stmt);
    sqlite3_bind_int64 (params->ins_relation_refs_stmt, 1,
			params->current_rel_id);
    sqlite3_bind_int (params->ins_relation_refs_stmt, 2,
		      params->current_rel_ref_sub);
    sqlite3_bind_text (params->ins_relation_refs_stmt, 3, type, strlen (type),
		       SQLITE_STATIC);
    sqlite3_bind_int64 (params->ins_relation_refs_stmt, 4, ref);
    sqlite3_bind_text (params->ins_relation_refs_stmt, 5, role, strlen (role),
		       SQLITE_STATIC);
    ret = sqlite3_step (params->ins_relation_refs_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_rel_refs += 1;
    params->current_rel_ref_sub += 1;
    return 1;
}

static int
insert_relation_tag (struct aux_params *params, const char *k, const char *v)
{
/* inserting a raw <relation><tag> into the DBMS */
    int ret;
    sqlite3_reset (params->ins_relation_tags_stmt);
    sqlite3_clear_bindings (params->ins_relation_tags_stmt);
    sqlite3_bind_int64 (params->ins_relation_tags_stmt, 1,
			params->current_rel_id);
    sqlite3_bind_int (params->ins_relation_tags_stmt, 2,
		      params->current_rel_tag_sub);
    sqlite3_bind_text (params->ins_relation_tags_stmt, 3, k, strlen (k),
		       SQLITE_STATIC);
    sqlite3_bind_text (params->ins_relation_tags_stmt, 4, v, strlen (v),
		       SQLITE_STATIC);
    ret = sqlite3_step (params->ins_relation_tags_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_rel_tags += 1;
    params->current_rel_tag_sub += 1;
    return 1;
}

static int
insert_node (struct aux_params *params, sqlite3_int64 id, double x, double y,
	     int version, const char *timestamp, int uid, int changeset,
	     const char *user)
{
/* inserting a raw <node> into the DBMS */
    int ret;
    unsigned char *blob;
    int blob_size;
    gaiaGeomCollPtr geom = gaiaAllocGeomColl ();
    geom->Srid = 4326;
    gaiaAddPointToGeomColl (geom, x, y);
    sqlite3_reset (params->ins_nodes_stmt);
    sqlite3_clear_bindings (params->ins_nodes_stmt);
    if (params->mode == MODE_RAW)
      {
	  sqlite3_bind_int64 (params->ins_nodes_stmt, 1, id);
	  sqlite3_bind_int (params->ins_nodes_stmt, 2, version);
	  if (timestamp == NULL)
	      sqlite3_bind_null (params->ins_nodes_stmt, 3);
	  else
	      sqlite3_bind_text (params->ins_nodes_stmt, 3, timestamp,
				 strlen (timestamp), SQLITE_STATIC);
	  sqlite3_bind_int (params->ins_nodes_stmt, 4, uid);
	  if (user == NULL)
	      sqlite3_bind_null (params->ins_nodes_stmt, 5);
	  else
	      sqlite3_bind_text (params->ins_nodes_stmt, 5, user, strlen (user),
				 SQLITE_STATIC);
	  sqlite3_bind_int (params->ins_nodes_stmt, 6, changeset);
	  if (!geom)
	      sqlite3_bind_null (params->ins_nodes_stmt, 7);
	  else
	    {
		gaiaToSpatiaLiteBlobWkb (geom, &blob, &blob_size);
		gaiaFreeGeomColl (geom);
		sqlite3_bind_blob (params->ins_nodes_stmt, 7, blob, blob_size,
				   free);
	    }
      }
    else
      {
	  sqlite3_bind_int64 (params->ins_nodes_stmt, 1, id);
	  if (!geom)
	      sqlite3_bind_null (params->ins_nodes_stmt, 2);
	  else
	    {
		gaiaToSpatiaLiteBlobWkb (geom, &blob, &blob_size);
		gaiaFreeGeomColl (geom);
		sqlite3_bind_blob (params->ins_nodes_stmt, 2, blob, blob_size,
				   free);
	    }
      }
    ret = sqlite3_step (params->ins_nodes_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_nodes += 1;
    params->current_node_id = id;
    params->current_node_tag_sub = 0;
    return 1;
}

static int
insert_way (struct aux_params *params, sqlite3_int64 id)
{
/* inserting a raw <way> into the DBMS */
    int ret;
    sqlite3_reset (params->ins_ways_stmt);
    sqlite3_clear_bindings (params->ins_ways_stmt);
    sqlite3_bind_int64 (params->ins_ways_stmt, 1, id);
    ret = sqlite3_step (params->ins_ways_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_ways += 1;
    params->current_way_id = id;
    params->current_way_ref_sub = 0;
    params->current_way_tag_sub = 0;
    return 1;
}

static int
insert_relation (struct aux_params *params, sqlite3_int64 id)
{
/* inserting a raw <relation> into the DBMS */
    int ret;
    sqlite3_reset (params->ins_relations_stmt);
    sqlite3_clear_bindings (params->ins_relations_stmt);
    sqlite3_bind_int64 (params->ins_relations_stmt, 1, id);
    ret = sqlite3_step (params->ins_relations_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	params->wr_relations += 1;
    params->current_rel_id = id;
    params->current_rel_ref_sub = 0;
    params->current_rel_tag_sub = 0;
    return 1;
}

const char *
parse_attribute_value (xmlNodePtr node)
{
/* parsing a string argument */
    if (node != NULL)
      {
	  if (node->type == XML_TEXT_NODE)
	      return (const char *) (node->content);
      }
    return NULL;
}

static int
parse_osm_nd_ref (struct _xmlAttr *attr, sqlite3_int64 * node_id)
{
/* parsing the OSM <node> attributes */
    const char *attr_ref = NULL;
    while (attr != NULL)
      {
	  if (attr->name != NULL)
	    {
		if (strcmp ((const char *) (attr->name), "ref") == 0)
		    attr_ref = parse_attribute_value (attr->children);
	    }
	  attr = attr->next;
      }
    if (attr_ref == NULL)
      {
	  fprintf (stderr, "Invalid OSM <nd>: ref=%s\n", attr_ref);
	  return 0;
      }
    *node_id = atol_64 (attr_ref);
    return 1;
}

static int
parse_osm_member (struct _xmlAttr *attr, const char **type, sqlite3_int64 * ref,
		  const char **role)
{
/* parsing the OSM <member> attributes */
    const char *attr_type = NULL;
    const char *attr_ref = NULL;
    const char *attr_role = NULL;
    while (attr != NULL)
      {
	  if (attr->name != NULL)
	    {
		if (strcmp ((const char *) (attr->name), "type") == 0)
		    attr_type = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "ref") == 0)
		    attr_ref = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "role") == 0)
		    attr_role = parse_attribute_value (attr->children);
	    }
	  attr = attr->next;
      }
    if (attr_type == NULL || attr_ref == NULL || attr_role == NULL)
      {
	  fprintf (stderr, "Invalid OSM <member>: type=%s ref=%s role=%s\n",
		   attr_type, attr_ref, attr_role);
	  return 0;
      }
    *type = attr_type;
    *ref = atol_64 (attr_ref);
    *role = attr_role;
    return 1;
}

static int
parse_osm_tag (struct _xmlAttr *attr, const char **k, const char **v)
{
/* parsing the OSM <tag> attributes */
    const char *attr_k = NULL;
    const char *attr_v = NULL;
    while (attr != NULL)
      {
	  if (attr->name != NULL)
	    {
		if (strcmp ((const char *) (attr->name), "k") == 0)
		    attr_k = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "v") == 0)
		    attr_v = parse_attribute_value (attr->children);
	    }
	  attr = attr->next;
      }
    if (attr_k == NULL || attr_v == NULL)
      {
	  fprintf (stderr, "Invalid OSM <tag>: k=%s v=%s\n", attr_k, attr_v);
	  return 0;
      }
    *k = attr_k;
    *v = attr_v;
    return 1;
}

static int
parse_osm_node_attributes (struct _xmlAttr *attr, sqlite3_int64 * id, double *x,
			   double *y, int *version, const char **timestamp,
			   int *uid, int *changeset, const char **user)
{
/* parsing the OSM <node> attributes */
    const char *attr_id = NULL;
    const char *attr_lon = NULL;
    const char *attr_lat = NULL;
    const char *attr_version = NULL;
    const char *attr_timestamp = NULL;
    const char *attr_uid = NULL;
    const char *attr_changeset = NULL;
    const char *attr_user = NULL;
    while (attr != NULL)
      {
	  if (attr->name != NULL)
	    {
		if (strcmp ((const char *) (attr->name), "id") == 0)
		    attr_id = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "lon") == 0)
		    attr_lon = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "lat") == 0)
		    attr_lat = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "version") == 0)
		    attr_version = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "timestamp") == 0)
		    attr_timestamp = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "uid") == 0)
		    attr_uid = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "changeset") == 0)
		    attr_changeset = parse_attribute_value (attr->children);
		if (strcmp ((const char *) (attr->name), "user") == 0)
		    attr_user = parse_attribute_value (attr->children);
	    }
	  attr = attr->next;
      }
    if (attr_id == NULL || attr_lon == NULL || attr_lat == NULL)
      {
	  fprintf (stderr, "Invalid OSM <node>: id=%s lat=%s lon=%s\n", attr_id,
		   attr_lon, attr_lat);
	  return 0;
      }
    *id = atol_64 (attr_id);
    *x = atof (attr_lon);
    *y = atof (attr_lat);
    if (attr_version == NULL)
	*version = -1;
    else
	*version = atoi (attr_version);
    *timestamp = attr_timestamp;
    if (attr_uid == NULL)
	*uid = -1;
    else
	*uid = atoi (attr_uid);
    if (attr_changeset == NULL)
	*changeset = -1;
    else
	*changeset = atoi (attr_changeset);
    *user = attr_user;
    return 1;
}

static int
parse_osm_way_attributes (struct _xmlAttr *attr, sqlite3_int64 * id)
{
/* parsing the OSM <way> attributes */
    const char *attr_id = NULL;
    while (attr != NULL)
      {
	  if (attr->name != NULL)
	    {
		if (strcmp ((const char *) (attr->name), "id") == 0)
		    attr_id = parse_attribute_value (attr->children);
	    }
	  attr = attr->next;
      }
    if (attr_id == NULL)
      {
	  fprintf (stderr, "Invalid OSM <way>: id=%s\n", attr_id);
	  return 0;
      }
    *id = atol_64 (attr_id);
    return 1;
}

static int
parse_osm_relation_attributes (struct _xmlAttr *attr, sqlite3_int64 * id)
{
/* parsing the OSM <relation> attributes */
    const char *attr_id = NULL;
    while (attr != NULL)
      {
	  if (attr->name != NULL)
	    {
		if (strcmp ((const char *) (attr->name), "id") == 0)
		    attr_id = parse_attribute_value (attr->children);
	    }
	  attr = attr->next;
      }
    if (attr_id == NULL)
      {
	  fprintf (stderr, "Invalid OSM <relation>: id=%s\n", attr_id);
	  return 0;
      }
    *id = atol_64 (attr_id);
    return 1;
}

static int
parse_osm_node_tag (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <node><tag> item */
    const char *k;
    const char *v;
    if (!parse_osm_tag (node->properties, &k, &v))
	return 0;
    if (!insert_node_tag (params, k, v))
	return 0;
    return 1;
}

static int
parse_osm_way_tag (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <way><tag> item */
    const char *k;
    const char *v;
    if (!parse_osm_tag (node->properties, &k, &v))
	return 0;
    if (!insert_way_tag (params, k, v))
	return 0;
    return 1;
}

static int
parse_osm_relation_tag (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <relation><tag> item */
    const char *k;
    const char *v;
    if (!parse_osm_tag (node->properties, &k, &v))
	return 0;
    if (!insert_relation_tag (params, k, v))
	return 0;
    return 1;
}

static int
parse_osm_way_ref (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <way><nd> item */
    sqlite3_int64 node_id;
    if (!parse_osm_nd_ref (node->properties, &node_id))
	return 0;
    if (!insert_way_ref (params, node_id))
	return 0;
    return 1;
}

static int
parse_osm_relation_member (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <relation><member> item */
    const char *type;
    sqlite3_int64 ref;
    const char *role;
    if (!parse_osm_member (node->properties, &type, &ref, &role))
	return 0;
    if (!insert_relation_ref (params, type, ref, role))
	return 0;
    return 1;
}

static int
parse_osm_node (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <node> item */
    xmlNodePtr child;
    sqlite3_int64 id;
    double x;
    double y;
    int version;
    const char *timestamp;
    int uid;
    int changeset;
    const char *user;
    int error = 0;

    if (!parse_osm_node_attributes
	(node->properties, &id, &x, &y, &version, &timestamp, &uid, &changeset,
	 &user))
	return 0;
    if (!insert_node
	(params, id, x, y, version, timestamp, uid, changeset, user))
	return 0;
    for (child = node->children; child; child = child->next)
      {
	  if (child->type == XML_ELEMENT_NODE)
	    {
		const char *name = (const char *) (child->name);
		if (name != NULL)
		  {
		      int ret = 1;
		      if (strcmp (name, "tag") == 0)
			  ret = parse_osm_node_tag (child, params);
		      if (!ret)
			  error = 1;
		  }
	    }
      }
    if (error)
	return 0;
    return 1;
}

static int
parse_osm_way (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <way> item */
    xmlNodePtr child;
    sqlite3_int64 id;
    int error = 0;

    if (!parse_osm_way_attributes (node->properties, &id))
	return 0;
    if (!insert_way (params, id))
	return 0;
    for (child = node->children; child; child = child->next)
      {
	  if (child->type == XML_ELEMENT_NODE)
	    {
		const char *name = (const char *) (child->name);
		if (name != NULL)
		  {
		      int ret = 1;
		      if (strcmp (name, "nd") == 0)
			  ret = parse_osm_way_ref (child, params);
		      if (strcmp (name, "tag") == 0)
			  ret = parse_osm_way_tag (child, params);
		      if (!ret)
			  error = 1;
		  }
	    }
      }
    if (error)
	return 0;
    return 1;
}

static int
parse_osm_relation (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing an OSM <relation> item */
    xmlNodePtr child;
    sqlite3_int64 id;
    int error = 0;

    if (!parse_osm_relation_attributes (node->properties, &id))
	return 0;
    if (!insert_relation (params, id))
	return 0;
    for (child = node->children; child; child = child->next)
      {
	  if (child->type == XML_ELEMENT_NODE)
	    {
		const char *name = (const char *) (child->name);
		if (name != NULL)
		  {
		      int ret = 1;
		      if (strcmp (name, "member") == 0)
			  ret = parse_osm_relation_member (child, params);
		      if (strcmp (name, "tag") == 0)
			  ret = parse_osm_relation_tag (child, params);
		      if (!ret)
			  error = 1;
		  }
	    }
      }
    if (error)
	return 0;
    return 1;
}

static int
parse_osm_items (xmlNodePtr node, struct aux_params *params)
{
/* recursively parsing the OSM payload */
    int error = 0;
    xmlNodePtr cur_node = NULL;

    for (cur_node = node; cur_node; cur_node = cur_node->next)
      {
	  if (cur_node->type == XML_ELEMENT_NODE)
	    {
		xmlNodePtr child;
		for (child = cur_node->children; child; child = child->next)
		  {
		      if (child->type == XML_ELEMENT_NODE)
			{
			    const char *name = (const char *) (child->name);
			    if (name != NULL)
			      {
				  int ret = 1;
				  if (strcmp (name, "node") == 0)
				      ret = parse_osm_node (child, params);
				  if (strcmp (name, "way") == 0)
				      ret = parse_osm_way (child, params);
				  if (strcmp (name, "relation") == 0)
				      ret = parse_osm_relation (child, params);
				  if (!ret)
				      error = 1;
			      }
			}
		  }
	    }
      }
    if (error)
	return 0;
    return 1;
}

static int
osm_parse (struct aux_params *params, struct download_tile *tile, int object)
{
    xmlDocPtr xml_doc = NULL;
    xmlNodePtr root;
    char *url;
    int error = 0;

    if (params->mode == MODE_ROAD)
      {
	  /* downloading only the ROAD network */
	  url =
	      sqlite3_mprintf
	      ("%s/interpreter?data=[timeout:600];(way[highway](%1.12f,%1.12f,%1.12f,%1.12f);>;);out body;",
	       params->osm_url, tile->miny, tile->minx, tile->maxy, tile->maxx);
      }
    else if (params->mode == MODE_RAIL)
      {
	  /* downloading only the RAILWAY network */
	  url =
	      sqlite3_mprintf
	      ("%s/interpreter?data=[timeout:600];(way[railway](%1.12f,%1.12f,%1.12f,%1.12f);>;);out body;",
	       params->osm_url, tile->miny, tile->minx, tile->maxy, tile->maxx);
      }
    else if (params->mode == MODE_MAP)
      {
	  /* downloading a full MAP */
	  if (object == OBJ_NODES)
	      url =
		  sqlite3_mprintf
		  ("%s/interpreter?data=[timeout:600];(node(%1.12f,%1.12f,%1.12f,%1.12f););out body;",
		   params->osm_url, tile->miny, tile->minx, tile->maxy,
		   tile->maxx);
	  else if (object == OBJ_WAYS)
	      url =
		  sqlite3_mprintf
		  ("%s/interpreter?data=[timeout:600];(way(%1.12f,%1.12f,%1.12f,%1.12f););out body;",
		   params->osm_url, tile->miny, tile->minx, tile->maxy,
		   tile->maxx);
	  else
	      url =
		  sqlite3_mprintf
		  ("%s/interpreter?data=[timeout:600];(relation(%1.12f,%1.12f,%1.12f,%1.12f););out body;",
		   params->osm_url, tile->miny, tile->minx, tile->maxy,
		   tile->maxx);
      }
    else
      {
	  /* downloading a full MAP - raw mode */
	  if (object == OBJ_NODES)
	      url =
		  sqlite3_mprintf
		  ("%s/interpreter?data=[timeout:600];(node(%1.12f,%1.12f,%1.12f,%1.12f););out meta;",
		   params->osm_url, tile->miny, tile->minx, tile->maxy,
		   tile->maxx);
	  else if (object == OBJ_WAYS)
	      url =
		  sqlite3_mprintf
		  ("%s/interpreter?data=[timeout:600];(way(%1.12f,%1.12f,%1.12f,%1.12f););out meta;",
		   params->osm_url, tile->miny, tile->minx, tile->maxy,
		   tile->maxx);
	  else
	      url =
		  sqlite3_mprintf
		  ("%s/interpreter?data=[timeout:600];(relation(%1.12f,%1.12f,%1.12f,%1.12f););out meta;",
		   params->osm_url, tile->miny, tile->minx, tile->maxy,
		   tile->maxx);
      }

    xml_doc = xmlReadFile (url, NULL, 0);
    if (xml_doc == NULL)
      {
	  /* parsing error; not a well-formed XML */
	  fprintf (stderr, "ERROR: unable to download the OSM dataset\n");
	  error = 1;
	  goto end;
      }

/* parsing the OSM payload */
    root = xmlDocGetRootElement (xml_doc);
    if (!parse_osm_items (root, params))
	error = 1;

  end:
    sqlite3_free (url);
    if (xml_doc != NULL)
	xmlFreeDoc (xml_doc);
    if (error)
	return 0;
    return 1;
}

static void
finalize_sql_stmts (struct aux_params *params)
{
    int ret;
    char *sql_err = NULL;

    if (params->ins_nodes_stmt != NULL)
	sqlite3_finalize (params->ins_nodes_stmt);
    if (params->ins_node_tags_stmt != NULL)
	sqlite3_finalize (params->ins_node_tags_stmt);
    if (params->ins_ways_stmt != NULL)
	sqlite3_finalize (params->ins_ways_stmt);
    if (params->ins_way_tags_stmt != NULL)
	sqlite3_finalize (params->ins_way_tags_stmt);
    if (params->ins_way_refs_stmt != NULL)
	sqlite3_finalize (params->ins_way_refs_stmt);
    if (params->ins_relations_stmt != NULL)
	sqlite3_finalize (params->ins_relations_stmt);
    if (params->ins_relation_tags_stmt != NULL)
	sqlite3_finalize (params->ins_relation_tags_stmt);
    if (params->ins_relation_refs_stmt != NULL)
	sqlite3_finalize (params->ins_relation_refs_stmt);

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
    sqlite3_stmt *ins_nodes_stmt;
    sqlite3_stmt *ins_node_tags_stmt;
    sqlite3_stmt *ins_ways_stmt;
    sqlite3_stmt *ins_way_tags_stmt;
    sqlite3_stmt *ins_way_refs_stmt;
    sqlite3_stmt *ins_relations_stmt;
    sqlite3_stmt *ins_relation_tags_stmt;
    sqlite3_stmt *ins_relation_refs_stmt;
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

/* the complete operation is handled as an unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }
    if (params->mode == MODE_RAW)
      {
	  strcpy (sql,
		  "INSERT INTO osm_nodes (node_id, version, timestamp, uid, user, changeset, Geometry) ");
	  strcat (sql, "VALUES (?, ?, ?, ?, ?, ?, ?)");
      }
    else
	strcpy (sql, "INSERT INTO osm_nodes (node_id, Geometry) VALUES (?, ?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "INSERT INTO osm_node_tags (node_id, sub, k, v) ");
    strcat (sql, "VALUES (?, ?, ?, ?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_node_tags_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "INSERT INTO osm_ways (way_id) VALUES (?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_ways_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "INSERT INTO osm_way_tags (way_id, sub, k, v) ");
    strcat (sql, "VALUES (?, ?, ?, ?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_way_tags_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "INSERT INTO osm_way_refs (way_id, sub, node_id) ");
    strcat (sql, "VALUES (?, ?, ?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_way_refs_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "INSERT INTO osm_relations (rel_id) VALUES (?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_relations_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql, "INSERT INTO osm_relation_tags (rel_id, sub, k, v) ");
    strcat (sql, "VALUES (?, ?, ?, ?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_relation_tags_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }
    strcpy (sql,
	    "INSERT INTO osm_relation_refs (rel_id, sub, type, ref, role) ");
    strcat (sql, "VALUES (?, ?, ?, ?, ?)");
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_relation_refs_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  finalize_sql_stmts (params);
	  return;
      }

    params->ins_nodes_stmt = ins_nodes_stmt;
    params->ins_node_tags_stmt = ins_node_tags_stmt;
    params->ins_ways_stmt = ins_ways_stmt;
    params->ins_way_tags_stmt = ins_way_tags_stmt;
    params->ins_way_refs_stmt = ins_way_refs_stmt;
    params->ins_relations_stmt = ins_relations_stmt;
    params->ins_relation_tags_stmt = ins_relation_tags_stmt;
    params->ins_relation_refs_stmt = ins_relation_refs_stmt;
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

static int
create_road_tables (struct aux_params *params)
{
/* creating the ROAD tables */
    sqlite3 *db_handle = params->db_handle;
    int ret;
    char sql[1024];
    char *err_msg = NULL;

/* creating OSM helper nodes */
    strcpy (sql, "CREATE TABLE osm_helper_nodes (\n");
    strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "way_count INTEGER NOT NULL)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_helper_nodes' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* populating OSM helper nodes */
    strcpy (sql, "INSERT INTO osm_helper_nodes (node_id, way_count) ");
    strcat (sql, "SELECT n.node_id, Count(*) FROM osm_nodes AS n ");
    strcat (sql, "JOIN osm_way_refs AS w ON (w.node_id = n.node_id) ");
    strcat (sql, "GROUP BY n.node_id");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "INSERT INTO 'osm_helper_nodes' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* creating ROAD nodes */
    strcpy (sql, "CREATE TABLE road_nodes (\n");
    strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'road_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql,
	    "SELECT AddGeometryColumn('road_nodes', 'Geometry', 4326, 'POINT', 'XY')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'road_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating ROAD arcs */
    strcpy (sql, "CREATE TABLE road_arcs (\n");
    strcat (sql, "arc_id INTEGER PRIMARY KEY AUTOINCREMENT,\n");
    strcat (sql, "osm_id INTEGER NOT NULL,\n");
    strcat (sql, "node_from INTEGER NOT NULL,\n");
    strcat (sql, "node_to INTEGER NOT NULL,\n");
    strcat (sql, "type TEXT,\n");
    strcat (sql, "name TEXT,\n");
    strcat (sql, "lanes INTEGER,\n");
    strcat (sql, "maxspeed INTEGER,\n");
    strcat (sql, "oneway_ft INTEGER,\n");
    strcat (sql, "oneway_tf INTEGER,\n");
    strcat (sql, "CONSTRAINT fk_arc_from FOREIGN KEY (node_from)\n");
    strcat (sql, "REFERENCES road_nodes (node_id),\n");
    strcat (sql, "CONSTRAINT fk_arc_to FOREIGN KEY (node_to)\n");
    strcat (sql, "REFERENCES road_nodes (node_id))\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'road_arcs' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql,
	    "SELECT AddGeometryColumn('road_arcs', 'Geometry', 4326, 'LINESTRING', 'XY')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'road_arcs' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating an index supporting road_arcs.node_from */
    strcpy (sql, "CREATE INDEX idx_roads_from ON road_arcs (node_from)");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'idx_roads_from' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating an index supporting road_arcs.node_to */
    strcpy (sql, "CREATE INDEX idx_roads_to ON road_arcs (node_to)");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'idx_roads_to' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_road_rtrees (struct aux_params *params)
{
/* creating the ROAD rtrees */
    sqlite3 *db_handle = params->db_handle;
    int ret;
    char sql[1024];
    char *err_msg = NULL;

    strcpy (sql, "SELECT CreateSpatialIndex('road_nodes', 'Geometry')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE SPATIAL INDEX 'road_nodes' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql, "SELECT CreateSpatialIndex('road_arcs', 'Geometry')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE SPATIAL INDEX 'road_arcs' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static void
add_arc (struct aux_arc_container *arcs, sqlite3_int64 nd_first,
	 sqlite3_int64 nd_last, double x_ini, double y_ini, double x_end,
	 double y_end, gaiaDynamicLinePtr dyn_line, int count)
{
/* adding a further Arc to the container */
    int iv;
    gaiaGeomCollPtr g;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    struct aux_arc *arc = malloc (sizeof (struct aux_arc));
    arc->node_from = nd_first;
    arc->node_to = nd_last;
    g = gaiaAllocGeomColl ();
    gaiaAddPointToGeomColl (g, x_ini, y_ini);
    g->Srid = 4326;
    arc->geom_from = g;
    g = gaiaAllocGeomColl ();
    gaiaAddPointToGeomColl (g, x_end, y_end);
    g->Srid = 4326;
    arc->geom_to = g;
    g = gaiaAllocGeomColl ();
    ln = gaiaAddLinestringToGeomColl (g, count);
    iv = 0;
    pt = dyn_line->First;
    while (pt)
      {
	  /* inserting any POINT into LINESTRING */
	  gaiaSetPoint (ln->Coords, iv, pt->X, pt->Y);
	  iv++;
	  pt = pt->Next;
      }
    g->Srid = 4326;
    arc->geom = g;
    arc->next = NULL;
    if (arcs->first == NULL)
	arcs->first = arc;
    if (arcs->last != NULL)
	arcs->last->next = arc;
    arcs->last = arc;
}

static int
build_arc (sqlite3 * sqlite, sqlite3_stmt * query_nodes_stmt, sqlite3_int64 id,
	   struct aux_arc_container *arcs)
{
/* building an Arc */
    int ret;
    sqlite3_int64 nd_first;
    sqlite3_int64 nd_last;
    int count = 0;
    double x_ini;
    double y_ini;
    double x_end;
    double y_end;
    gaiaDynamicLinePtr dyn_line = gaiaAllocDynamicLine ();

    sqlite3_reset (query_nodes_stmt);
    sqlite3_clear_bindings (query_nodes_stmt);
    sqlite3_bind_int64 (query_nodes_stmt, 1, id);
    while (1)
      {
	  /* scrolling the main result set */
	  ret = sqlite3_step (query_nodes_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		sqlite3_int64 node_id =
		    sqlite3_column_int64 (query_nodes_stmt, 0);
		double x = sqlite3_column_double (query_nodes_stmt, 1);
		double y = sqlite3_column_double (query_nodes_stmt, 2);
		int way_count = sqlite3_column_int (query_nodes_stmt, 3);
		if (count == 0)
		  {
		      nd_first = node_id;
		      x_ini = x;
		      y_ini = y;
		      count++;
		      gaiaAppendPointToDynamicLine (dyn_line, x, y);
		  }
		else
		  {
		      nd_last = node_id;
		      x_end = x;
		      y_end = y;
		      count++;
		      gaiaAppendPointToDynamicLine (dyn_line, x, y);
		      if (way_count > 1)
			{
			    /* break: splitting the current arc on some junction */
			    add_arc (arcs, nd_first, nd_last, x_ini, y_ini,
				     x_end, y_end, dyn_line, count);
			    /* beginning a new arc */
			    gaiaFreeDynamicLine (dyn_line);
			    dyn_line = gaiaAllocDynamicLine ();
			    count = 1;
			    nd_first = node_id;
			    x_ini = x;
			    y_ini = y;
			    gaiaAppendPointToDynamicLine (dyn_line, x, y);
			}
		  }
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (sqlite));
		return 0;
	    }
      }
    if (count > 1)
	add_arc (arcs, nd_first, nd_last, x_ini, y_ini, x_end, y_end, dyn_line,
		 count);
    gaiaFreeDynamicLine (dyn_line);
    return 1;
}

static int
populate_road_network (struct aux_params *params, int *cnt_nodes, int *cnt_arcs)
{
/* populating the ROAD tables */
    int ret;
    sqlite3_stmt *query_main_stmt = NULL;
    sqlite3_stmt *query_nodes_stmt = NULL;
    sqlite3_stmt *ins_nodes_stmt = NULL;
    sqlite3_stmt *ins_arcs_stmt = NULL;
    const char *sql;
    char *sql_err = NULL;

/* main SQL query extracting all Arcs */
    sql = "SELECT w1.way_id AS osm_id, w1.v AS class, w2.v AS name, "
	"w3.v AS lanes, w4.v AS maxspeed, w5.v AS onewway, w6.v AS roundabout "
	"FROM osm_way_tags AS w1 "
	"LEFT JOIN osm_way_tags AS w2 ON (w2.way_id = w1.way_id AND w2.k = 'name') "
	"LEFT JOIN osm_way_tags AS w3 ON (w3.way_id = w1.way_id AND w3.k = 'lanes') "
	"LEFT JOIN osm_way_tags AS w4 ON (w4.way_id = w1.way_id AND w4.k = 'maxspeed') "
	"LEFT JOIN osm_way_tags AS w5 ON (w5.way_id = w1.way_id AND w5.k = 'oneway') "
	"LEFT JOIN osm_way_tags AS w6 ON (w6.way_id = w1.way_id AND w6.k = 'junction') "
	"WHERE w1.k = 'highway'";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_main_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* aux SQL query extracting Node refs */
    sql = "SELECT w.node_id, ST_X(n.geometry), ST_Y(n.geometry), h.way_count "
	"FROM osm_way_refs AS w "
	"JOIN osm_nodes AS n ON (n.node_id = w.node_id) "
	"JOIN osm_helper_nodes AS h ON (n.node_id = h.node_id) "
	"WHERE w.way_id = ? ORDER BY w.sub";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* INSERT INTO nodes statement */
    sql = "INSERT INTO road_nodes (node_id, geometry) VALUES (?, ?)";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* INSERT INTO arcs statement */
    sql =
	"INSERT INTO road_arcs (arc_id, osm_id, node_from, node_to, type, name, lanes, "
	"maxspeed, oneway_ft, oneway_tf, geometry) VALUES (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_arcs_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* the complete operation is handled as an unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  goto error;
      }

    while (1)
      {
	  /* scrolling the main result set */
	  ret = sqlite3_step (query_main_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		struct aux_arc_container arcs;
		struct aux_arc *arc;
		struct aux_arc *arc_n;
		unsigned char *blob;
		int blob_size;
		sqlite3_int64 id = sqlite3_column_int64 (query_main_stmt, 0);
		arcs.first = NULL;
		arcs.last = NULL;
		if (!build_arc (params->db_handle, query_nodes_stmt, id, &arcs))
		  {
#if defined(_WIN32) || defined(__MINGW32__)
		      /* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
		      fprintf (stderr,
			       "ERROR: unable to resolve ROAD id=%I64d\n", id);
#else
		      fprintf (stderr,
			       "ERROR: unable to resolve ROAD id=%lld\n", id);
#endif
		      goto error;
		  }
		arc = arcs.first;
		while (arc != NULL)
		  {
		      int oneway_ft = 1;
		      int oneway_tf = 1;
		      const char *p_oneway = "";
		      const char *p_roundabout = "";
		      const char *p_motorway = "";
		      /* looping on split arcs */
		      arc_n = arc->next;
		      /* inserting NODE From */
		      sqlite3_reset (ins_nodes_stmt);
		      sqlite3_clear_bindings (ins_nodes_stmt);
		      sqlite3_bind_int64 (ins_nodes_stmt, 1, arc->node_from);
		      gaiaToSpatiaLiteBlobWkb (arc->geom_from, &blob,
					       &blob_size);
		      gaiaFreeGeomColl (arc->geom_from);
		      sqlite3_bind_blob (ins_nodes_stmt, 2, blob, blob_size,
					 free);
		      ret = sqlite3_step (ins_nodes_stmt);
		      if (ret == SQLITE_DONE || ret == SQLITE_ROW)
			  *cnt_nodes += 1;
		      /* inserting NODE To */
		      sqlite3_reset (ins_nodes_stmt);
		      sqlite3_clear_bindings (ins_nodes_stmt);
		      sqlite3_bind_int64 (ins_nodes_stmt, 1, arc->node_to);
		      gaiaToSpatiaLiteBlobWkb (arc->geom_to, &blob, &blob_size);
		      gaiaFreeGeomColl (arc->geom_to);
		      sqlite3_bind_blob (ins_nodes_stmt, 2, blob, blob_size,
					 free);
		      ret = sqlite3_step (ins_nodes_stmt);
		      if (ret == SQLITE_DONE || ret == SQLITE_ROW)
			  *cnt_nodes += 1;
		      /* inserting the Arc itself */
		      sqlite3_reset (ins_arcs_stmt);
		      sqlite3_clear_bindings (ins_arcs_stmt);
		      sqlite3_bind_int64 (ins_arcs_stmt, 1, id);
		      sqlite3_bind_int64 (ins_arcs_stmt, 2, arc->node_from);
		      sqlite3_bind_int64 (ins_arcs_stmt, 3, arc->node_to);
		      if (sqlite3_column_type (query_main_stmt, 1) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 4);
		      else
			  sqlite3_bind_text (ins_arcs_stmt, 4,
					     (const char *)
					     sqlite3_column_text
					     (query_main_stmt, 1),
					     sqlite3_column_bytes
					     (query_main_stmt, 1),
					     SQLITE_STATIC);
		      if (sqlite3_column_type (query_main_stmt, 2) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 5);
		      else
			  sqlite3_bind_text (ins_arcs_stmt, 5,
					     (const char *)
					     sqlite3_column_text
					     (query_main_stmt, 2),
					     sqlite3_column_bytes
					     (query_main_stmt, 2),
					     SQLITE_STATIC);
		      if (sqlite3_column_type (query_main_stmt, 3) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 6);
		      else
			  sqlite3_bind_int (ins_arcs_stmt, 6,
					    sqlite3_column_int (query_main_stmt,
								3));
		      if (sqlite3_column_type (query_main_stmt, 4) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 7);
		      else
			  sqlite3_bind_int (ins_arcs_stmt, 7,
					    sqlite3_column_int (query_main_stmt,
								4));
		      if (sqlite3_column_type (query_main_stmt, 5) !=
			  SQLITE_NULL)
			  p_oneway =
			      (const char *)
			      sqlite3_column_text (query_main_stmt, 5);
		      if (sqlite3_column_type (query_main_stmt, 6) !=
			  SQLITE_NULL)
			  p_roundabout =
			      (const char *)
			      sqlite3_column_text (query_main_stmt, 6);
		      if (sqlite3_column_type (query_main_stmt, 1) !=
			  SQLITE_NULL)
			  p_motorway =
			      (const char *)
			      sqlite3_column_text (query_main_stmt, 1);
		      if (strcmp (p_roundabout, "roundabout") == 0)
			{
			    /* all roundabouts are always implicitly oneway */
			    oneway_ft = 1;
			    oneway_tf = 0;
			}
		      if (strcmp (p_motorway, "motorway") == 0)
			{
			    /* all motorways are always implicitly oneway */
			    oneway_ft = 1;
			    oneway_tf = 0;
			}
		      if (strcmp (p_oneway, "1") == 0
			  || strcmp (p_oneway, "yes") == 0)
			{
			    /* declared to be oneway From -> To */
			    oneway_ft = 1;
			    oneway_tf = 0;
			}
		      if (strcmp (p_oneway, "-1") == 0
			  || strcmp (p_oneway, "reverse") == 0)
			{
			    /* declared to be oneway To -> From */
			    oneway_ft = 0;
			    oneway_tf = 1;
			}
		      sqlite3_bind_int (ins_arcs_stmt, 8, oneway_ft);
		      sqlite3_bind_int (ins_arcs_stmt, 9, oneway_tf);
		      gaiaToSpatiaLiteBlobWkb (arc->geom, &blob, &blob_size);
		      gaiaFreeGeomColl (arc->geom);
		      sqlite3_bind_blob (ins_arcs_stmt, 10, blob, blob_size,
					 free);
		      ret = sqlite3_step (ins_arcs_stmt);
		      if (ret == SQLITE_DONE || ret == SQLITE_ROW)
			  *cnt_arcs += 1;
		      else
			{
#if defined(_WIN32) || defined(__MINGW32__)
			    /* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
			    fprintf (stderr,
				     "ERROR: unable to insert ROAD id=%I64d: %s\n",
				     id, sqlite3_errmsg (params->db_handle));
#else
			    fprintf (stderr,
				     "ERROR: unable to insert ROAD id=%lld: %s\n",
				     id, sqlite3_errmsg (params->db_handle));
#endif
			}
		      free (arc);
		      arc = arc_n;
		  }
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		/* ROLLBACK */
		ret =
		    sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL,
				  &sql_err);
		if (ret != SQLITE_OK)
		  {
		      fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n",
			       sql_err);
		      sqlite3_free (sql_err);
		      goto error;
		  }
		goto error;
	    }
      }

/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  goto error;
      }

    sqlite3_finalize (query_main_stmt);
    sqlite3_finalize (query_nodes_stmt);
    sqlite3_finalize (ins_nodes_stmt);
    sqlite3_finalize (ins_arcs_stmt);

    return 1;

  error:
    if (query_main_stmt != NULL)
	sqlite3_finalize (query_main_stmt);
    if (query_nodes_stmt != NULL)
	sqlite3_finalize (query_nodes_stmt);
    if (ins_nodes_stmt != NULL)
	sqlite3_finalize (ins_nodes_stmt);
    if (ins_arcs_stmt != NULL)
	sqlite3_finalize (ins_arcs_stmt);
    return 0;
}

static int
create_rail_tables (struct aux_params *params)
{
/* creating the RAIL tables */
    sqlite3 *db_handle = params->db_handle;
    int ret;
    char sql[1024];
    char *err_msg = NULL;

/* creating OSM helper nodes */
    strcpy (sql, "CREATE TABLE osm_helper_nodes (\n");
    strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "way_count INTEGER NOT NULL)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_helper_nodes' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* populating OSM helper nodes */
    strcpy (sql, "INSERT INTO osm_helper_nodes (node_id, way_count) ");
    strcat (sql, "SELECT n.node_id, Count(*) FROM osm_nodes AS n ");
    strcat (sql, "JOIN osm_way_refs AS w ON (w.node_id = n.node_id) ");
    strcat (sql, "GROUP BY n.node_id");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "INSERT INTO 'osm_helper_nodes' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* creating RAIL nodes */
    strcpy (sql, "CREATE TABLE rail_nodes (\n");
    strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'rail_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql,
	    "SELECT AddGeometryColumn('rail_nodes', 'Geometry', 4326, 'POINT', 'XY')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'rail_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating RAIL arcs */
    strcpy (sql, "CREATE TABLE rail_arcs (\n");
    strcat (sql, "arc_id INTEGER PRIMARY KEY AUTOINCREMENT,\n");
    strcat (sql, "osm_id INTEGER NOT NULL,\n");
    strcat (sql, "node_from INTEGER NOT NULL,\n");
    strcat (sql, "node_to INTEGER NOT NULL,\n");
    strcat (sql, "type TEXT,\n");
    strcat (sql, "name TEXT,\n");
    strcat (sql, "gauge INTEGER,\n");
    strcat (sql, "tracks INTEGER,\n");
    strcat (sql, "electrified INTEGER,\n");
    strcat (sql, "voltage INTEGER,\n");
    strcat (sql, "operator TEXT,\n");
    strcat (sql, "CONSTRAINT fk_arc_from FOREIGN KEY (node_from)\n");
    strcat (sql, "REFERENCES rail_nodes (node_id),\n");
    strcat (sql, "CONSTRAINT fk_arc_to FOREIGN KEY (node_to)\n");
    strcat (sql, "REFERENCES rail_nodes (node_id))\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'rail_arcs' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql,
	    "SELECT AddGeometryColumn('rail_arcs', 'Geometry', 4326, 'LINESTRING', 'XY')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'rail_arcs' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating an index supporting rail_arcs.node_from */
    strcpy (sql, "CREATE INDEX idx_rails_from ON rail_arcs (node_from)");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'idx_rails_from' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating an index supporting rail_arcs.node_to */
    strcpy (sql, "CREATE INDEX idx_rails_to ON rail_arcs (node_to)");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'idx_rails_to' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* creating RAIL stations */
    strcpy (sql, "CREATE TABLE rail_stations (\n");
    strcat (sql, "station_id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "name TEXT\n,");
    strcat (sql, "operator TEXT)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'rail_stations' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql,
	    "SELECT AddGeometryColumn('rail_stations', 'Geometry', 4326, 'POINT', 'XY')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'rail_stations' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
create_rail_rtrees (struct aux_params *params)
{
/* creating the RAIL rtrees */
    sqlite3 *db_handle = params->db_handle;
    int ret;
    char sql[1024];
    char *err_msg = NULL;

    strcpy (sql, "SELECT CreateSpatialIndex('rail_nodes', 'Geometry')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE SPATIAL INDEX 'rail_nodes' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql, "SELECT CreateSpatialIndex('rail_arcs', 'Geometry')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE SPATIAL INDEX 'rail_arcs' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql, "SELECT CreateSpatialIndex('rail_stations', 'Geometry')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE SPATIAL INDEX 'rail_stations' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static int
populate_rail_network (struct aux_params *params, int *cnt_nodes, int *cnt_arcs,
		       int *cnt_stations)
{
/* populating the RAIL tables */
    int ret;
    sqlite3_stmt *query_main_stmt = NULL;
    sqlite3_stmt *query_nodes_stmt = NULL;
    sqlite3_stmt *query_stations_stmt = NULL;
    sqlite3_stmt *ins_nodes_stmt = NULL;
    sqlite3_stmt *ins_arcs_stmt = NULL;
    sqlite3_stmt *ins_stations_stmt = NULL;
    const char *sql;
    char *sql_err = NULL;

/* main SQL query extracting all Arcs */
    sql =
	"SELECT w1.way_id AS osm_id, w1.v AS class, w2.v AS name, w3.v AS gauge, "
	"w4.v AS tracks, w5.v AS electrified, w6.v AS voltage, w7.v AS operator "
	"FROM osm_way_tags AS w1 "
	"LEFT JOIN osm_way_tags AS w2 ON (w2.way_id = w1.way_id AND w2.k = 'name') "
	"LEFT JOIN osm_way_tags AS w3 ON (w3.way_id = w1.way_id AND w3.k = 'gauge') "
	"LEFT JOIN osm_way_tags AS w4 ON (w4.way_id = w1.way_id AND w4.k = 'tracks') "
	"LEFT JOIN osm_way_tags AS w5 ON (w5.way_id = w1.way_id AND w5.k = 'electrified') "
	"LEFT JOIN osm_way_tags AS w6 ON (w6.way_id = w1.way_id AND w6.k = 'voltage') "
	"LEFT JOIN osm_way_tags AS w7 ON (w7.way_id = w1.way_id AND w7.k = 'operator') "
	"WHERE w1.k = 'railway'";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_main_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* aux SQL query extracting Node refs */
    sql = "SELECT w.node_id, ST_X(n.geometry), ST_Y(n.geometry), h.way_count "
	"FROM osm_way_refs AS w "
	"JOIN osm_nodes AS n ON (n.node_id = w.node_id) "
	"JOIN osm_helper_nodes AS h ON (n.node_id = h.node_id) "
	"WHERE w.way_id = ? ORDER BY w.sub";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* main SQL query extracting all Stations */
    sql =
	"SELECT n1.node_id AS osm_id, n2.v AS name, n3.v AS operator, g.geometry "
	"FROM osm_node_tags AS n1 "
	"JOIN osm_nodes AS g ON (g.node_id = n1.node_id) "
	"LEFT JOIN osm_node_tags AS n2 ON (n2.node_id = n1.node_id AND n2.k = 'name') "
	"LEFT JOIN osm_node_tags AS n3 ON (n3.node_id = n1.node_id AND n3.k = 'operator') "
	"WHERE n1.k = 'railway' AND n1.v = 'station'";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_stations_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* INSERT INTO nodes statement */
    sql = "INSERT INTO rail_nodes (node_id, geometry) VALUES (?, ?)";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* INSERT INTO arcs statement */
    sql =
	"INSERT INTO rail_arcs (arc_id, osm_id, node_from, node_to, type, name, "
	"gauge, tracks, electrified, voltage, operator, geometry) VALUES "
	"(NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_arcs_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* INSERT INTO stations statement */
    sql =
	"INSERT INTO rail_stations (station_id, name, operator, geometry) VALUES (?, ?, ?, ?)";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &ins_stations_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* the complete operation is handled as an unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  goto error;
      }

    while (1)
      {
	  /* scrolling the main result set */
	  ret = sqlite3_step (query_main_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		struct aux_arc_container arcs;
		struct aux_arc *arc;
		struct aux_arc *arc_n;
		unsigned char *blob;
		int blob_size;
		sqlite3_int64 id = sqlite3_column_int64 (query_main_stmt, 0);
		arcs.first = NULL;
		arcs.last = NULL;
		if (!build_arc (params->db_handle, query_nodes_stmt, id, &arcs))
		  {
#if defined(_WIN32) || defined(__MINGW32__)
		      /* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
		      fprintf (stderr,
			       "ERROR: unable to resolve RAIL id=%I64d\n", id);
#else
		      fprintf (stderr,
			       "ERROR: unable to resolve RAIL id=%lld\n", id);
#endif
		      goto error;
		  }
		arc = arcs.first;
		while (arc != NULL)
		  {
		      /* looping on split arcs */
		      arc_n = arc->next;
		      /* inserting NODE From */
		      sqlite3_reset (ins_nodes_stmt);
		      sqlite3_clear_bindings (ins_nodes_stmt);
		      sqlite3_bind_int64 (ins_nodes_stmt, 1, arc->node_from);
		      gaiaToSpatiaLiteBlobWkb (arc->geom_from, &blob,
					       &blob_size);
		      gaiaFreeGeomColl (arc->geom_from);
		      sqlite3_bind_blob (ins_nodes_stmt, 2, blob, blob_size,
					 free);
		      ret = sqlite3_step (ins_nodes_stmt);
		      if (ret == SQLITE_DONE || ret == SQLITE_ROW)
			  *cnt_nodes += 1;
		      /* inserting NODE To */
		      sqlite3_reset (ins_nodes_stmt);
		      sqlite3_clear_bindings (ins_nodes_stmt);
		      sqlite3_bind_int64 (ins_nodes_stmt, 1, arc->node_to);
		      gaiaToSpatiaLiteBlobWkb (arc->geom_to, &blob, &blob_size);
		      gaiaFreeGeomColl (arc->geom_to);
		      sqlite3_bind_blob (ins_nodes_stmt, 2, blob, blob_size,
					 free);
		      ret = sqlite3_step (ins_nodes_stmt);
		      if (ret == SQLITE_DONE || ret == SQLITE_ROW)
			  *cnt_nodes += 1;
		      /* inserting the Arc itself */
		      sqlite3_reset (ins_arcs_stmt);
		      sqlite3_clear_bindings (ins_arcs_stmt);
		      sqlite3_bind_int64 (ins_arcs_stmt, 1, id);
		      sqlite3_bind_int64 (ins_arcs_stmt, 2, arc->node_from);
		      sqlite3_bind_int64 (ins_arcs_stmt, 3, arc->node_to);
		      if (sqlite3_column_type (query_main_stmt, 1) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 4);
		      else
			  sqlite3_bind_text (ins_arcs_stmt, 4,
					     (const char *)
					     sqlite3_column_text
					     (query_main_stmt, 1),
					     sqlite3_column_bytes
					     (query_main_stmt, 1),
					     SQLITE_STATIC);
		      if (sqlite3_column_type (query_main_stmt, 2) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 5);
		      else
			  sqlite3_bind_text (ins_arcs_stmt, 5,
					     (const char *)
					     sqlite3_column_text
					     (query_main_stmt, 2),
					     sqlite3_column_bytes
					     (query_main_stmt, 2),
					     SQLITE_STATIC);
		      if (sqlite3_column_type (query_main_stmt, 3) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 6);
		      else
			  sqlite3_bind_int (ins_arcs_stmt, 6,
					    sqlite3_column_int (query_main_stmt,
								3));
		      if (sqlite3_column_type (query_main_stmt, 4) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 7);
		      else
			  sqlite3_bind_int (ins_arcs_stmt, 7,
					    sqlite3_column_int (query_main_stmt,
								4));
		      if (sqlite3_column_type (query_main_stmt, 5) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 8);
		      else
			  sqlite3_bind_text (ins_arcs_stmt, 8,
					     (const char *)
					     sqlite3_column_text
					     (query_main_stmt, 5),
					     sqlite3_column_bytes
					     (query_main_stmt, 5),
					     SQLITE_STATIC);
		      if (sqlite3_column_type (query_main_stmt, 6) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 9);
		      else
			  sqlite3_bind_int (ins_arcs_stmt, 9,
					    sqlite3_column_int (query_main_stmt,
								6));
		      if (sqlite3_column_type (query_main_stmt, 6) ==
			  SQLITE_NULL)
			  sqlite3_bind_null (ins_arcs_stmt, 10);
		      else
			  sqlite3_bind_text (ins_arcs_stmt, 10,
					     (const char *)
					     sqlite3_column_text
					     (query_main_stmt, 7),
					     sqlite3_column_bytes
					     (query_main_stmt, 7),
					     SQLITE_STATIC);
		      gaiaToSpatiaLiteBlobWkb (arc->geom, &blob, &blob_size);
		      gaiaFreeGeomColl (arc->geom);
		      sqlite3_bind_blob (ins_arcs_stmt, 11, blob, blob_size,
					 free);
		      ret = sqlite3_step (ins_arcs_stmt);
		      if (ret == SQLITE_DONE || ret == SQLITE_ROW)
			  *cnt_arcs += 1;
		      else
			{
#if defined(_WIN32) || defined(__MINGW32__)
			    /* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
			    fprintf (stderr,
				     "ERROR: unable to insert RAIL id=%I64d: %s\n",
				     id, sqlite3_errmsg (params->db_handle));
#else
			    fprintf (stderr,
				     "ERROR: unable to insert RAIL id=%lld: %s\n",
				     id, sqlite3_errmsg (params->db_handle));
#endif
			}
		      free (arc);
		      arc = arc_n;
		  }
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		/* ROLLBACK */
		ret =
		    sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL,
				  &sql_err);
		if (ret != SQLITE_OK)
		  {
		      fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n",
			       sql_err);
		      sqlite3_free (sql_err);
		      goto error;
		  }
		goto error;
	    }
      }

    while (1)
      {
	  /* scrolling the stations result set */
	  ret = sqlite3_step (query_stations_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		sqlite3_int64 id =
		    sqlite3_column_int64 (query_stations_stmt, 0);
		sqlite3_reset (ins_stations_stmt);
		sqlite3_clear_bindings (ins_stations_stmt);
		sqlite3_bind_int64 (ins_stations_stmt, 1, id);
		if (sqlite3_column_type (query_stations_stmt, 1) == SQLITE_NULL)
		    sqlite3_bind_null (ins_stations_stmt, 2);
		else
		    sqlite3_bind_text (ins_stations_stmt, 2,
				       (const char *)
				       sqlite3_column_text (query_stations_stmt,
							    1),
				       sqlite3_column_bytes
				       (query_stations_stmt, 1), SQLITE_STATIC);
		if (sqlite3_column_type (query_stations_stmt, 2) == SQLITE_NULL)
		    sqlite3_bind_null (ins_stations_stmt, 3);
		else
		    sqlite3_bind_text (ins_stations_stmt, 3,
				       (const char *)
				       sqlite3_column_text (query_stations_stmt,
							    2),
				       sqlite3_column_bytes
				       (query_stations_stmt, 2), SQLITE_STATIC);
		if (sqlite3_column_type (query_stations_stmt, 3) == SQLITE_NULL)
		    sqlite3_bind_null (ins_stations_stmt, 4);
		else
		    sqlite3_bind_blob (ins_stations_stmt, 4,
				       sqlite3_column_blob (query_stations_stmt,
							    3),
				       sqlite3_column_bytes
				       (query_stations_stmt, 3), SQLITE_STATIC);
		ret = sqlite3_step (ins_stations_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    *cnt_stations += 1;
		else
		  {
#if defined(_WIN32) || defined(__MINGW32__)
		      /* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
		      fprintf (stderr,
			       "ERROR: unable to insert RAIL station id=%I64d: %s\n",
			       id, sqlite3_errmsg (params->db_handle));
#else
		      fprintf (stderr,
			       "ERROR: unable to insert RAIL station id=%lld: %s\n",
			       id, sqlite3_errmsg (params->db_handle));
#endif
		  }
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		/* ROLLBACK */
		ret =
		    sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL,
				  &sql_err);
		if (ret != SQLITE_OK)
		  {
		      fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n",
			       sql_err);
		      sqlite3_free (sql_err);
		      goto error;
		  }
		goto error;
	    }
      }

/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  goto error;
      }

    sqlite3_finalize (query_main_stmt);
    sqlite3_finalize (query_nodes_stmt);
    sqlite3_finalize (ins_nodes_stmt);
    sqlite3_finalize (ins_arcs_stmt);

    return 1;

  error:
    if (query_main_stmt != NULL)
	sqlite3_finalize (query_main_stmt);
    if (query_nodes_stmt != NULL)
	sqlite3_finalize (query_nodes_stmt);
    if (query_stations_stmt != NULL)
	sqlite3_finalize (query_stations_stmt);
    if (ins_nodes_stmt != NULL)
	sqlite3_finalize (ins_nodes_stmt);
    if (ins_arcs_stmt != NULL)
	sqlite3_finalize (ins_arcs_stmt);
    if (ins_stations_stmt != NULL)
	sqlite3_finalize (ins_stations_stmt);
    return 0;
}

static int
create_map_indices (struct aux_params *params)
{
/* creating the MAP helper indices */
    sqlite3 *db_handle = params->db_handle;
    int ret;
    char sql[1024];
    char *err_msg = NULL;

/* creating OSM helper idx_node_tags */
    strcpy (sql, "CREATE INDEX idx_node_tags ON osm_node_tags (k)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'osm_node_tags' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* creating OSM helper idx_way_tags */
    strcpy (sql, "CREATE INDEX idx_way_tags ON osm_node_tags (k)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'osm_way_tags' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* creating OSM helper idx_relation_tags */
    strcpy (sql, "CREATE INDEX idx_relation_tags ON osm_node_tags (k)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'osm_relation_tags' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static void
do_create_point_table (struct aux_params *params, struct layers *layer)
{
/* creating a Point table */
    int ret;
    char *err_msg = NULL;
    char *sql;
    sqlite3_stmt *stmt;

    sql = sqlite3_mprintf ("CREATE TABLE pt_%s (\n"
			   "osm_id INTEGER NOT NULL PRIMARY KEY,\n"
			   "type TEXT,\nname TEXT)\n", layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'pt_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }
    sql =
	sqlite3_mprintf
	("SELECT AddGeometryColumn('pt_%s', 'Geometry', 4326, 'POINT', 'XY')",
	 layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'pt_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }

/* creating the insert SQL statement */
    sql = sqlite3_mprintf ("INSERT INTO pt_%s (osm_id, type, name, Geometry) "
			   "VALUES (?, ?, ?, ?)", layer->name);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  return;
      }
    layer->ok_point = 1;
    layer->ins_point_stmt = stmt;
}

static int
do_insert_point (struct aux_params *params, sqlite3_int64 id,
		 const char *layer_name, const char *type, const char *name,
		 const unsigned char *blob, int blob_size)
{
/* inserting a Point Geometry */
    struct layers *p_layer;
    struct layers *layer;
    int i = 0;
    int ret;

    layer = NULL;
    while (1)
      {
	  p_layer = &(base_layers[i++]);
	  if (p_layer->name == NULL)
	      break;
	  if (strcmp (p_layer->name, layer_name) == 0)
	    {
		layer = p_layer;
		break;
	    }
      }
    if (layer == NULL)
      {
	  fprintf (stderr, "Unknown Point Layer: %s\n", layer_name);
	  return 0;
      }

    if (layer->ok_point == 0)
	do_create_point_table (params, layer);

    if (layer->ok_point && layer->ins_point_stmt != NULL)
      {
	  sqlite3_reset (layer->ins_point_stmt);
	  sqlite3_clear_bindings (layer->ins_point_stmt);
	  sqlite3_bind_int64 (layer->ins_point_stmt, 1, id);
	  if (type == NULL)
	      sqlite3_bind_null (layer->ins_point_stmt, 2);
	  else
	      sqlite3_bind_text (layer->ins_point_stmt, 2, type, strlen (type),
				 SQLITE_STATIC);
	  if (name == NULL)
	      sqlite3_bind_null (layer->ins_point_stmt, 3);
	  else
	      sqlite3_bind_text (layer->ins_point_stmt, 3, name, strlen (name),
				 SQLITE_STATIC);
	  if (blob == NULL)
	      sqlite3_bind_null (layer->ins_point_stmt, 4);
	  else
	      sqlite3_bind_blob (layer->ins_point_stmt, 4, blob, blob_size,
				 SQLITE_STATIC);
	  ret = sqlite3_step (layer->ins_point_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      return 1;
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
      }
    return 0;
}

static void
do_create_linestring_table (struct aux_params *params, struct layers *layer)
{
/* creating a Linestring table */
    int ret;
    char *err_msg = NULL;
    char *sql;
    sqlite3_stmt *stmt;

    sql = sqlite3_mprintf ("CREATE TABLE ln_%s (\n"
			   "osm_id INTEGER NOT NULL PRIMARY KEY,\n"
			   "type TEXT,\nname TEXT)\n", layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'ln_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }
    sql =
	sqlite3_mprintf
	("SELECT AddGeometryColumn('ln_%s', 'Geometry', 4326, 'LINESTRING', 'XY')",
	 layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'ln_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }

/* creating the insert SQL statement */
    sql = sqlite3_mprintf ("INSERT INTO ln_%s (osm_id, type, name, Geometry) "
			   "VALUES (?, ?, ?, ?)", layer->name);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  return;
      }
    layer->ok_linestring = 1;
    layer->ins_linestring_stmt = stmt;
}

static int
do_insert_linestring (struct aux_params *params, sqlite3_int64 id,
		      const char *layer_name, const char *type,
		      const char *name, gaiaGeomCollPtr geom)
{
/* inserting a Linestring Geometry */
    struct layers *p_layer;
    struct layers *layer;
    int i = 0;
    int ret;

    layer = NULL;
    while (1)
      {
	  p_layer = &(base_layers[i++]);
	  if (p_layer->name == NULL)
	      break;
	  if (strcmp (p_layer->name, layer_name) == 0)
	    {
		layer = p_layer;
		break;
	    }
      }
    if (layer == NULL)
      {
	  fprintf (stderr, "Unknown Linestring Layer: %s\n", layer_name);
	  return 0;
      }

    if (layer->ok_linestring == 0)
	do_create_linestring_table (params, layer);

    if (layer->ok_linestring && layer->ins_linestring_stmt != NULL)
      {
	  unsigned char *blob;
	  int blob_size;
	  sqlite3_reset (layer->ins_linestring_stmt);
	  sqlite3_clear_bindings (layer->ins_linestring_stmt);
	  sqlite3_bind_int64 (layer->ins_linestring_stmt, 1, id);
	  if (type == NULL)
	      sqlite3_bind_null (layer->ins_linestring_stmt, 2);
	  else
	      sqlite3_bind_text (layer->ins_linestring_stmt, 2, type,
				 strlen (type), SQLITE_STATIC);
	  if (name == NULL)
	      sqlite3_bind_null (layer->ins_linestring_stmt, 3);
	  else
	      sqlite3_bind_text (layer->ins_linestring_stmt, 3, name,
				 strlen (name), SQLITE_STATIC);
	  gaiaToSpatiaLiteBlobWkb (geom, &blob, &blob_size);
	  if (blob == NULL)
	      sqlite3_bind_null (layer->ins_linestring_stmt, 4);
	  else
	      sqlite3_bind_blob (layer->ins_linestring_stmt, 4, blob, blob_size,
				 free);
	  ret = sqlite3_step (layer->ins_linestring_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      return 1;
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
      }
    return 0;
}

static void
do_create_polygon_table (struct aux_params *params, struct layers *layer)
{
/* creating a Polygon table */
    int ret;
    char *err_msg = NULL;
    char *sql;
    sqlite3_stmt *stmt;

    sql = sqlite3_mprintf ("CREATE TABLE pg_%s (\n"
			   "osm_id INTEGER NOT NULL PRIMARY KEY,\n"
			   "type TEXT,\nname TEXT)\n", layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'pg_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }
    sql =
	sqlite3_mprintf
	("SELECT AddGeometryColumn('pg_%s', 'Geometry', 4326, 'POLYGON', 'XY')",
	 layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'pg_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }

/* creating the insert SQL statement */
    sql = sqlite3_mprintf ("INSERT INTO pg_%s (osm_id, type, name, Geometry) "
			   "VALUES (?, ?, ?, ?)", layer->name);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  return;
      }
    layer->ok_polygon = 1;
    layer->ins_polygon_stmt = stmt;
}

static int
do_insert_polygon (struct aux_params *params, sqlite3_int64 id,
		   const char *layer_name, const char *type, const char *name,
		   gaiaGeomCollPtr geom)
{
/* inserting a Polygon Geometry */
    struct layers *p_layer;
    struct layers *layer;
    int i = 0;
    int ret;

    layer = NULL;
    while (1)
      {
	  p_layer = &(base_layers[i++]);
	  if (p_layer->name == NULL)
	      break;
	  if (strcmp (p_layer->name, layer_name) == 0)
	    {
		layer = p_layer;
		break;
	    }
      }
    if (layer == NULL)
      {
	  fprintf (stderr, "Unknown Polygon Layer: %s\n", layer_name);
	  return 0;
      }

    if (layer->ok_polygon == 0)
	do_create_polygon_table (params, layer);

    if (layer->ok_polygon && layer->ins_polygon_stmt != NULL)
      {
	  unsigned char *blob;
	  int blob_size;
	  sqlite3_reset (layer->ins_polygon_stmt);
	  sqlite3_clear_bindings (layer->ins_polygon_stmt);
	  sqlite3_bind_int64 (layer->ins_polygon_stmt, 1, id);
	  if (type == NULL)
	      sqlite3_bind_null (layer->ins_polygon_stmt, 2);
	  else
	      sqlite3_bind_text (layer->ins_polygon_stmt, 2, type,
				 strlen (type), SQLITE_STATIC);
	  if (name == NULL)
	      sqlite3_bind_null (layer->ins_polygon_stmt, 3);
	  else
	      sqlite3_bind_text (layer->ins_polygon_stmt, 3, name,
				 strlen (name), SQLITE_STATIC);
	  gaiaToSpatiaLiteBlobWkb (geom, &blob, &blob_size);
	  if (blob == NULL)
	      sqlite3_bind_null (layer->ins_polygon_stmt, 4);
	  else
	      sqlite3_bind_blob (layer->ins_polygon_stmt, 4, blob, blob_size,
				 free);
	  ret = sqlite3_step (layer->ins_polygon_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      return 1;
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
      }
    return 0;
}

static void
do_create_multi_linestring_table (struct aux_params *params,
				  struct layers *layer)
{
/* creating a MultiLinestring table */
    int ret;
    char *err_msg = NULL;
    char *sql;
    sqlite3_stmt *stmt;

    sql = sqlite3_mprintf ("CREATE TABLE mln_%s (\n"
			   "node_id INTEGER NOT NULL PRIMARY KEY,\n"
			   "type TEXT,\nname TEXT)\n", layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'mln_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }
    sql =
	sqlite3_mprintf
	("SELECT AddGeometryColumn('mln_%s', 'Geometry', 4326, 'MULTILINESTRING', 'XY')",
	 layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'mln_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }

/* creating the insert SQL statement */
    sql = sqlite3_mprintf ("INSERT INTO mln_%s (node_id, type, name, Geometry) "
			   "VALUES (?, ?, ?, ?)", layer->name);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  return;
      }
    layer->ok_multi_linestring = 1;
    layer->ins_multi_linestring_stmt = stmt;
}

static int
do_insert_multi_linestring (struct aux_params *params, sqlite3_int64 id,
			    const char *layer_name, const char *type,
			    const char *name, gaiaGeomCollPtr geom)
{
/* inserting a MultiLinestring Geometry */
    struct layers *p_layer;
    struct layers *layer;
    int i = 0;
    int ret;

    layer = NULL;
    while (1)
      {
	  p_layer = &(base_layers[i++]);
	  if (p_layer->name == NULL)
	      break;
	  if (strcmp (p_layer->name, layer_name) == 0)
	    {
		layer = p_layer;
		break;
	    }
      }
    if (layer == NULL)
      {
	  fprintf (stderr, "Unknown MultiLinestring Layer: %s\n", layer_name);
	  return 0;
      }

    if (layer->ok_multi_linestring == 0)
	do_create_multi_linestring_table (params, layer);

    if (layer->ok_multi_linestring && layer->ins_multi_linestring_stmt != NULL)
      {
	  unsigned char *blob;
	  int blob_size;
	  sqlite3_reset (layer->ins_multi_linestring_stmt);
	  sqlite3_clear_bindings (layer->ins_multi_linestring_stmt);
	  sqlite3_bind_int64 (layer->ins_multi_linestring_stmt, 1, id);
	  if (type == NULL)
	      sqlite3_bind_null (layer->ins_multi_linestring_stmt, 2);
	  else
	      sqlite3_bind_text (layer->ins_multi_linestring_stmt, 2, type,
				 strlen (type), SQLITE_STATIC);
	  if (name == NULL)
	      sqlite3_bind_null (layer->ins_multi_linestring_stmt, 3);
	  else
	      sqlite3_bind_text (layer->ins_multi_linestring_stmt, 3, name,
				 strlen (name), SQLITE_STATIC);
	  gaiaToSpatiaLiteBlobWkb (geom, &blob, &blob_size);
	  if (blob == NULL)
	      sqlite3_bind_null (layer->ins_multi_linestring_stmt, 4);
	  else
	      sqlite3_bind_blob (layer->ins_multi_linestring_stmt, 4, blob,
				 blob_size, free);
	  ret = sqlite3_step (layer->ins_multi_linestring_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      return 1;
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
      }
    return 0;
}

static void
do_create_multi_polygon_table (struct aux_params *params, struct layers *layer)
{
/* creating a MultiPolygon table */
    int ret;
    char *err_msg = NULL;
    char *sql;
    sqlite3_stmt *stmt;

    sql = sqlite3_mprintf ("CREATE TABLE mpg_%s (\n"
			   "node_id INTEGER NOT NULL PRIMARY KEY,\n"
			   "type TEXT,\nname TEXT)\n", layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'mpg_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }
    sql =
	sqlite3_mprintf
	("SELECT AddGeometryColumn('mpg_%s', 'Geometry', 4326, 'MULTIPOLYGON', 'XY')",
	 layer->name);
    ret = sqlite3_exec (params->db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'mpg_%s' error: %s\n", layer->name,
		   err_msg);
	  sqlite3_free (err_msg);
	  return;
      }

/* creating the insert SQL statement */
    sql = sqlite3_mprintf ("INSERT INTO mpg_%s (node_id, type, name, Geometry) "
			   "VALUES (?, ?, ?, ?)", layer->name);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql,
		   sqlite3_errmsg (params->db_handle));
	  return;
      }
    layer->ok_multi_polygon = 1;
    layer->ins_multi_polygon_stmt = stmt;
}

static int
do_insert_multi_polygon (struct aux_params *params, sqlite3_int64 id,
			 const char *layer_name, const char *type,
			 const char *name, gaiaGeomCollPtr geom)
{
/* inserting a MultiPolygon Geometry */
    struct layers *p_layer;
    struct layers *layer;
    int i = 0;
    int ret;

    layer = NULL;
    while (1)
      {
	  p_layer = &(base_layers[i++]);
	  if (p_layer->name == NULL)
	      break;
	  if (strcmp (p_layer->name, layer_name) == 0)
	    {
		layer = p_layer;
		break;
	    }
      }
    if (layer == NULL)
      {
	  fprintf (stderr, "Unknown MultiPolygon Layer: %s\n", layer_name);
	  return 0;
      }

    if (layer->ok_multi_polygon == 0)
	do_create_multi_polygon_table (params, layer);

    if (layer->ok_multi_polygon && layer->ins_multi_polygon_stmt != NULL)
      {
	  unsigned char *blob;
	  int blob_size;
	  sqlite3_reset (layer->ins_multi_polygon_stmt);
	  sqlite3_clear_bindings (layer->ins_multi_polygon_stmt);
	  sqlite3_bind_int64 (layer->ins_multi_polygon_stmt, 1, id);
	  if (type == NULL)
	      sqlite3_bind_null (layer->ins_multi_polygon_stmt, 2);
	  else
	      sqlite3_bind_text (layer->ins_multi_polygon_stmt, 2, type,
				 strlen (type), SQLITE_STATIC);
	  if (name == NULL)
	      sqlite3_bind_null (layer->ins_multi_polygon_stmt, 3);
	  else
	      sqlite3_bind_text (layer->ins_multi_polygon_stmt, 3, name,
				 strlen (name), SQLITE_STATIC);
	  gaiaToSpatiaLiteBlobWkb (geom, &blob, &blob_size);
	  if (blob == NULL)
	      sqlite3_bind_null (layer->ins_multi_polygon_stmt, 4);
	  else
	      sqlite3_bind_blob (layer->ins_multi_polygon_stmt, 4, blob,
				 blob_size, free);
	  ret = sqlite3_step (layer->ins_multi_polygon_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      return 1;
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
      }
    return 0;
}

static int
build_way_geom (sqlite3 * sqlite, sqlite3_stmt * query_nodes_stmt,
		sqlite3_int64 id, const char *layer_name, int polygon,
		gaiaGeomCollPtr * p_geom)
{
/* building a Way Geometry */
    int ret;
    int areal_layer = 0;
    int is_closed = 0;
    int count = 0;
    double x0;
    double y0;
    double xN;
    double yN;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    gaiaPolygonPtr pg;
    gaiaRingPtr rng;
    int iv;
    gaiaGeomCollPtr geom;
    gaiaDynamicLinePtr dyn_line = gaiaAllocDynamicLine ();

    if (layer_name)
      {
	  /* possible "areal" layers */
	  if (strcmp (layer_name, "amenity") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "building") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "historic") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "landuse") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "leisure") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "natural") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "parking") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "place") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "shop") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "sport") == 0)
	      areal_layer = 1;
	  if (strcmp (layer_name, "tourism") == 0)
	      areal_layer = 1;
      }

    sqlite3_reset (query_nodes_stmt);
    sqlite3_clear_bindings (query_nodes_stmt);
    sqlite3_bind_int64 (query_nodes_stmt, 1, id);
    while (1)
      {
	  /* scrolling the main result set */
	  ret = sqlite3_step (query_nodes_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		double x = sqlite3_column_double (query_nodes_stmt, 0);
		double y = sqlite3_column_double (query_nodes_stmt, 1);
		gaiaAppendPointToDynamicLine (dyn_line, x, y);
		if (count == 0)
		  {
		      x0 = x;
		      y0 = y;
		  }
		else
		  {
		      xN = x;
		      yN = y;
		  }
		count++;
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (sqlite));
		return 0;
	    }
      }

/* testing for a closed ring */
    if (x0 == xN && y0 == yN)
	is_closed = 1;

    geom = gaiaAllocGeomColl ();
    if (is_closed && (polygon || areal_layer))
      {
	  /* building a Polygon Geometry */
	  pg = gaiaAddPolygonToGeomColl (geom, count, 0);
	  rng = pg->Exterior;
	  iv = 0;
	  pt = dyn_line->First;
	  while (pt)
	    {
		/* inserting any POINT into LINESTRING */
		gaiaSetPoint (rng->Coords, iv, pt->X, pt->Y);
		iv++;
		pt = pt->Next;
	    }
	  geom->DeclaredType = GAIA_POLYGON;
      }
    else
      {
	  /* building a Linestring Geometry */
	  ln = gaiaAddLinestringToGeomColl (geom, count);
	  iv = 0;
	  pt = dyn_line->First;
	  while (pt)
	    {
		/* inserting any POINT into LINESTRING */
		gaiaSetPoint (ln->Coords, iv, pt->X, pt->Y);
		iv++;
		pt = pt->Next;
	    }
	  geom->DeclaredType = GAIA_LINESTRING;
      }
    geom->Srid = 4326;
    gaiaFreeDynamicLine (dyn_line);
    *p_geom = geom;
    return 1;
}

static int
build_rel_way_geom (void *cache, sqlite3 * sqlite,
		    sqlite3_stmt * query_nodes_stmt, sqlite3_int64 id,
		    gaiaGeomCollPtr * p_geom)
{
/* building a complex Relation-Way Geometry */
    int ret;
    int count = 0;
    gaiaPointPtr pt;
    gaiaLinestringPtr ln;
    int iv;
    int first = 1;
    sqlite3_int64 current_id;
    gaiaGeomCollPtr geom;
    gaiaDynamicLinePtr dyn_line = gaiaAllocDynamicLine ();
    gaiaGeomCollPtr aggregate_geom = gaiaAllocGeomColl ();
    aggregate_geom->Srid = 4326;

    sqlite3_reset (query_nodes_stmt);
    sqlite3_clear_bindings (query_nodes_stmt);
    sqlite3_bind_int64 (query_nodes_stmt, 1, id);
    while (1)
      {
	  /* scrolling the main result set */
	  ret = sqlite3_step (query_nodes_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		sqlite3_int64 way_id =
		    sqlite3_column_int64 (query_nodes_stmt, 0);
		double x = sqlite3_column_double (query_nodes_stmt, 1);
		double y = sqlite3_column_double (query_nodes_stmt, 2);
		if (first)
		  {
		      current_id = way_id;
		      first = 0;
		  }
		if (way_id != current_id)
		  {
		      /* saving the current Way Linestring */
		      ln = gaiaAddLinestringToGeomColl (aggregate_geom, count);
		      iv = 0;
		      pt = dyn_line->First;
		      while (pt)
			{
			    /* inserting any POINT into LINESTRING */
			    gaiaSetPoint (ln->Coords, iv, pt->X, pt->Y);
			    iv++;
			    pt = pt->Next;
			}
		      gaiaFreeDynamicLine (dyn_line);
		      dyn_line = gaiaAllocDynamicLine ();
		      count = 0;
		      current_id = way_id;
		  }
		gaiaAppendPointToDynamicLine (dyn_line, x, y);
		count++;
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (sqlite));
		return 0;
	    }
      }

/* saving the last Way Linestring */
    ln = gaiaAddLinestringToGeomColl (aggregate_geom, count);
    iv = 0;
    pt = dyn_line->First;
    while (pt)
      {
	  /* inserting any POINT into LINESTRING */
	  gaiaSetPoint (ln->Coords, iv, pt->X, pt->Y);
	  iv++;
	  pt = pt->Next;
      }
    gaiaFreeDynamicLine (dyn_line);

/* attempting to build a MultiPolygon */
    geom = gaiaPolygonize_r (cache, aggregate_geom, 1);
    if (geom != NULL)
      {
	  geom->Srid = 4326;
	  geom->DeclaredType = GAIA_MULTIPOLYGON;
	  gaiaFreeGeomColl (aggregate_geom);
	  *p_geom = geom;
	  return 1;
      }

/* attempting to build a MultiLinestring */
    geom = gaiaLineMerge_r (cache, aggregate_geom);
    if (geom != NULL)
      {
	  geom->Srid = 4326;
	  geom->DeclaredType = GAIA_MULTILINESTRING;
	  gaiaFreeGeomColl (aggregate_geom);
	  *p_geom = geom;
	  return 1;
      }

/* returning the aggregate geom as is */
    aggregate_geom->DeclaredType = GAIA_MULTILINESTRING;
    *p_geom = aggregate_geom;
    return 1;
}

static int
populate_map_layers (struct aux_params *params, int *points, int *linestrings,
		     int *polygons, int *multi_linestrings, int *multi_polygons)
{
/* populating the MAP layers aka tables */
    int ret;
    sqlite3_stmt *query_nodes_stmt = NULL;
    sqlite3_stmt *query_ways_stmt = NULL;
    sqlite3_stmt *query_rel_ways_stmt = NULL;
    sqlite3_stmt *query_way_nodes_stmt = NULL;
    sqlite3_stmt *query_rel_way_nodes_stmt = NULL;
    char *layers;
    char *sql;
    char *sql_err = NULL;
    struct layers *layer;
    int i = 0;

/* preparing the list of well known layers */
    while (1)
      {
	  layer = &(base_layers[i++]);
	  if (layer->name == NULL)
	      break;
	  if (i == 1)
	      layers = sqlite3_mprintf ("%Q", layer->name);
	  else
	    {
		char *prev = layers;
		layers = sqlite3_mprintf ("%s, %Q", prev, layer->name);
		sqlite3_free (prev);
	    }
      }

/* main SQL query extracting all relevant Nodes */
    sql =
	sqlite3_mprintf ("SELECT n1.node_id, n1.k AS class, n1.v AS subclass, "
			 "n2.v AS name, g.geometry AS geometry "
			 "FROM osm_node_tags AS n1 "
			 "JOIN osm_nodes AS g ON (g.node_id = n1.node_id) "
			 "LEFT JOIN osm_node_tags AS n2 ON (n2.node_id = n1.node_id AND n2.k = 'name') "
			 "WHERE n1.k IN (%s)", layers);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_nodes_stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* main SQL query extracting all relevant Ways */
    sql = sqlite3_mprintf ("SELECT w1.way_id, w1.k AS class, w1.v AS subclass, "
			   "w2.v AS name, w3.v AS polygon "
			   "FROM osm_way_tags AS w1 "
			   "LEFT JOIN osm_way_tags AS w2 ON (w2.way_id = w1.way_id AND w2.k = 'name') "
			   "LEFT JOIN osm_way_tags AS w3 ON (w3.way_id = w1.way_id AND w3.k = 'area') "
			   "WHERE w1.k IN (%s)", layers);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_ways_stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* main SQL query extracting all relevant Relations-Ways */
    sql = sqlite3_mprintf ("SELECT r1.rel_id, r1.k AS class, r1.v AS subclass, "
			   "r2.v AS name, count(rr.ref) AS cnt FROM osm_relation_tags AS r1 "
			   "LEFT JOIN osm_relation_tags AS r2 ON (r2.rel_id = r1.rel_id AND r2.k = 'name') "
			   "LEFT JOIN osm_relation_refs AS rr ON (rr.rel_id = r1.rel_id AND rr.type = 'way') "
			   "WHERE r1.k IN (%s) GROUP BY r1.rel_id", layers);
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_rel_ways_stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }
    sqlite3_free (layers);

/* aux SQL query extracting Way-Node refs */
    sql = "SELECT ST_X(n.geometry), ST_Y(n.geometry) "
	"FROM osm_way_refs AS w "
	"JOIN osm_nodes AS n ON (n.node_id = w.node_id) "
	"WHERE w.way_id = ? ORDER BY w.sub";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_way_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* aux SQL query extracting Relation-Way-Node refs */
    sql = "SELECT w.way_id, ST_X(n.geometry), ST_Y(n.geometry) "
	"FROM osm_way_refs AS w "
	"JOIN osm_nodes AS n ON (n.node_id = w.node_id) "
	"WHERE w.way_id IN (SELECT ref FROM osm_relation_refs "
	"WHERE rel_id = ? AND type = 'way') " "ORDER BY w.way_id, w.sub";
    ret =
	sqlite3_prepare_v2 (params->db_handle, sql, strlen (sql),
			    &query_rel_way_nodes_stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n",
		   sqlite3_errmsg (params->db_handle));
	  goto error;
      }

/* the complete operation is handled as an unique SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  goto error;
      }

    while (1)
      {
	  /* scrolling the Nodes result set */
	  ret = sqlite3_step (query_nodes_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		const char *layer = NULL;
		const char *type = NULL;
		const char *name = NULL;
		const unsigned char *blob = NULL;
		int blob_size;
		sqlite3_int64 id = sqlite3_column_int64 (query_nodes_stmt, 0);
		if (sqlite3_column_type (query_nodes_stmt, 1) != SQLITE_NULL)
		    layer =
			(const char *) sqlite3_column_text (query_nodes_stmt,
							    1);
		if (sqlite3_column_type (query_nodes_stmt, 2) != SQLITE_NULL)
		    type =
			(const char *) sqlite3_column_text (query_nodes_stmt,
							    2);
		if (sqlite3_column_type (query_nodes_stmt, 3) != SQLITE_NULL)
		    name =
			(const char *) sqlite3_column_text (query_nodes_stmt,
							    3);
		if (sqlite3_column_type (query_nodes_stmt, 4) == SQLITE_BLOB)
		  {
		      blob =
			  (const unsigned char *)
			  sqlite3_column_blob (query_nodes_stmt, 4);
		      blob_size = sqlite3_column_bytes (query_nodes_stmt, 4);
		  }
		if (do_insert_point
		    (params, id, layer, type, name, blob, blob_size))
		    *points += 1;
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		/* ROLLBACK */
		ret =
		    sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL,
				  &sql_err);
		if (ret != SQLITE_OK)
		  {
		      fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n",
			       sql_err);
		      sqlite3_free (sql_err);
		      goto error;
		  }
		goto error;
	    }
      }

    while (1)
      {
	  /* scrolling the Ways result set */
	  ret = sqlite3_step (query_ways_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		const char *layer = NULL;
		const char *type = NULL;
		const char *name = NULL;
		int polygon = 0;
		gaiaGeomCollPtr geom = NULL;
		sqlite3_int64 id = sqlite3_column_int64 (query_ways_stmt, 0);
		if (sqlite3_column_type (query_ways_stmt, 1) != SQLITE_NULL)
		    layer =
			(const char *) sqlite3_column_text (query_ways_stmt, 1);
		if (sqlite3_column_type (query_ways_stmt, 2) != SQLITE_NULL)
		    type =
			(const char *) sqlite3_column_text (query_ways_stmt, 2);
		if (sqlite3_column_type (query_ways_stmt, 3) != SQLITE_NULL)
		    name =
			(const char *) sqlite3_column_text (query_ways_stmt, 3);
		if (sqlite3_column_type (query_ways_stmt, 4) != SQLITE_NULL)
		  {
		      if (strcmp
			  ((const char *)
			   sqlite3_column_text (query_ways_stmt, 4),
			   "yes") == 0)
			  polygon = 1;
		  }
		if (!build_way_geom
		    (params->db_handle, query_way_nodes_stmt, id, layer,
		     polygon, &geom))
		  {
#if defined(_WIN32) || defined(__MINGW32__)
		      /* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
		      fprintf (stderr,
			       "ERROR: unable to resolve WAY id=%I64d\n", id);
#else
		      fprintf (stderr,
			       "ERROR: unable to resolve WAY id=%lld\n", id);
#endif
		      goto error;
		  }
		if (geom->DeclaredType == GAIA_LINESTRING)
		  {
		      if (do_insert_linestring
			  (params, id, layer, type, name, geom))
			  *linestrings += 1;
		  }
		else
		  {
		      if (do_insert_polygon
			  (params, id, layer, type, name, geom))
			  *polygons += 1;
		  }
		gaiaFreeGeomColl (geom);
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		/* ROLLBACK */
		ret =
		    sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL,
				  &sql_err);
		if (ret != SQLITE_OK)
		  {
		      fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n",
			       sql_err);
		      sqlite3_free (sql_err);
		      goto error;
		  }
		goto error;
	    }
      }
    while (1)
      {
	  /* scrolling the Relations-Ways result set */
	  ret = sqlite3_step (query_rel_ways_stmt);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		const char *layer = NULL;
		const char *type = NULL;
		const char *name = NULL;
		gaiaGeomCollPtr geom = NULL;
		sqlite3_int64 id =
		    sqlite3_column_int64 (query_rel_ways_stmt, 0);
		if (sqlite3_column_type (query_rel_ways_stmt, 1) != SQLITE_NULL)
		    layer =
			(const char *) sqlite3_column_text (query_rel_ways_stmt,
							    1);
		if (sqlite3_column_type (query_rel_ways_stmt, 2) != SQLITE_NULL)
		    type =
			(const char *) sqlite3_column_text (query_rel_ways_stmt,
							    2);
		if (sqlite3_column_type (query_rel_ways_stmt, 3) != SQLITE_NULL)
		    name =
			(const char *) sqlite3_column_text (query_rel_ways_stmt,
							    3);
		if (!build_rel_way_geom
		    (params->cache, params->db_handle, query_rel_way_nodes_stmt,
		     id, &geom))
		  {
#if defined(_WIN32) || defined(__MINGW32__)
		      /* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
		      fprintf (stderr,
			       "ERROR: unable to resolve RELATION-WAY id=%I64d\n",
			       id);
#else
		      fprintf (stderr,
			       "ERROR: unable to resolve RELATION-WAY id=%lld\n",
			       id);
#endif
		      goto error;
		  }
		if (geom->DeclaredType == GAIA_MULTILINESTRING)
		  {
		      if (do_insert_multi_linestring
			  (params, id, layer, type, name, geom))
			  *multi_linestrings += 1;
		  }
		else
		  {
		      if (do_insert_multi_polygon
			  (params, id, layer, type, name, geom))
			  *multi_polygons += 1;
		  }
		gaiaFreeGeomColl (geom);
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (params->db_handle));
		/* ROLLBACK */
		ret =
		    sqlite3_exec (params->db_handle, "ROLLBACK", NULL, NULL,
				  &sql_err);
		if (ret != SQLITE_OK)
		  {
		      fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n",
			       sql_err);
		      sqlite3_free (sql_err);
		      goto error;
		  }
		goto error;
	    }
      }

/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (params->db_handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  goto error;
      }

    sqlite3_finalize (query_nodes_stmt);
    sqlite3_finalize (query_ways_stmt);
    sqlite3_finalize (query_way_nodes_stmt);
    sqlite3_finalize (query_rel_way_nodes_stmt);

    return 1;

  error:
    if (query_nodes_stmt != NULL)
	sqlite3_finalize (query_nodes_stmt);
    if (query_ways_stmt != NULL)
	sqlite3_finalize (query_ways_stmt);
    if (query_way_nodes_stmt != NULL)
	sqlite3_finalize (query_way_nodes_stmt);
    if (query_rel_way_nodes_stmt != NULL)
	sqlite3_finalize (query_rel_way_nodes_stmt);
    return 0;
}

static void
do_clean_roads (struct aux_params *params)
{
/* ROAD post-processing: DB cleanup */
    sqlite3 *db_handle = params->db_handle;
    printf ("\nFinal DBMS cleanup\n");
    sqlite3_exec (db_handle, "DROP TABLE osm_helper_nodes", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relation_refs", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relation_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relations", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_way_refs", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_way_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_ways", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_node_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle,
		  "DELETE FROM geometry_columns WHERE f_table_name = 'osm_nodes'",
		  NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_nodes", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "VACUUM", NULL, NULL, NULL);
    printf ("All done\n");
}

static void
do_clean_rails (struct aux_params *params)
{
/* ROAD post-processing: DB cleanup */
    sqlite3 *db_handle = params->db_handle;
    printf ("\nFinal DBMS cleanup\n");
    sqlite3_exec (db_handle, "DROP TABLE osm_helper_nodes", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relation_refs", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relation_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relations", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_way_refs", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_way_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_ways", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_node_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle,
		  "DELETE FROM geometry_columns WHERE f_table_name = 'osm_nodes'",
		  NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_nodes", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "VACUUM", NULL, NULL, NULL);
    printf ("All done\n");
}

static void
do_clean_map (struct aux_params *params)
{
/* MAP post-processing: DB cleanup */
    sqlite3 *db_handle = params->db_handle;
    printf ("\nFinal DBMS cleanup\n");
    sqlite3_exec (db_handle, "DROP TABLE osm_relation_refs", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relation_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_relations", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_way_refs", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_way_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_ways", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_node_tags", NULL, NULL, NULL);
    sqlite3_exec (db_handle,
		  "DELETE FROM geometry_columns WHERE f_table_name = 'osm_nodes'",
		  NULL, NULL, NULL);
    sqlite3_exec (db_handle, "DROP TABLE osm_nodes", NULL, NULL, NULL);
    sqlite3_exec (db_handle, "VACUUM", NULL, NULL, NULL);
    printf ("All done\n");
}

static int
create_osm_raw_tables (struct aux_params *params)
{
/* creating the raw OSM tables */
    sqlite3 *db_handle = params->db_handle;
    int ret;
    char sql[1024];
    char *err_msg = NULL;

/* creating the OSM "raw" nodes */
    if (params->mode == MODE_RAW)
      {
	  strcpy (sql, "CREATE TABLE osm_nodes (\n");
	  strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY,\n");
	  strcat (sql, "version INTEGER,\n");
	  strcat (sql, "timestamp TEXT,\n");
	  strcat (sql, "uid INTEGER,\n");
	  strcat (sql, "user TEXT,\n");
	  strcat (sql, "changeset INTEGER)\n");
      }
    else
      {
	  strcpy (sql, "CREATE TABLE osm_nodes (\n");
	  strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY)\n");
      }
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    strcpy (sql,
	    "SELECT AddGeometryColumn('osm_nodes', 'Geometry', 4326, 'POINT', 'XY')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating the OSM "raw" node tags */
    strcpy (sql, "CREATE TABLE osm_node_tags (\n");
    strcat (sql, "node_id INTEGER NOT NULL,\n");
    strcat (sql, "sub INTEGER NOT NULL,\n");
    strcat (sql, "k TEXT,\n");
    strcat (sql, "v TEXT,\n");
    strcat (sql, "CONSTRAINT pk_osm_nodetags PRIMARY KEY (node_id, sub),\n");
    strcat (sql, "CONSTRAINT fk_osm_nodetags FOREIGN KEY (node_id) ");
    strcat (sql, "REFERENCES osm_nodes (node_id))\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_node_tags' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating the OSM "raw" ways */
    strcpy (sql, "CREATE TABLE osm_ways (\n");
    strcat (sql, "way_id INTEGER NOT NULL PRIMARY KEY)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_ways' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating the OSM "raw" way tags */
    strcpy (sql, "CREATE TABLE osm_way_tags (\n");
    strcat (sql, "way_id INTEGER NOT NULL,\n");
    strcat (sql, "sub INTEGER NOT NULL,\n");
    strcat (sql, "k TEXT,\n");
    strcat (sql, "v TEXT,\n");
    strcat (sql, "CONSTRAINT pk_osm_waytags PRIMARY KEY (way_id, sub),\n");
    strcat (sql, "CONSTRAINT fk_osm_waytags FOREIGN KEY (way_id) ");
    strcat (sql, "REFERENCES osm_ways (way_id))\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_way_tags' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating the OSM "raw" way-node refs */
    strcpy (sql, "CREATE TABLE osm_way_refs (\n");
    strcat (sql, "way_id INTEGER NOT NULL,\n");
    strcat (sql, "sub INTEGER NOT NULL,\n");
    strcat (sql, "node_id INTEGER NOT NULL,\n");
    strcat (sql, "CONSTRAINT pk_osm_waynoderefs PRIMARY KEY (way_id, sub),\n");
    strcat (sql, "CONSTRAINT fk_osm_waynoderefs FOREIGN KEY (way_id) ");
    strcat (sql, "REFERENCES osm_ways (way_id))\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_way_refs' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating an index supporting osm_way_refs.node_id */
    strcpy (sql, "CREATE INDEX idx_osm_ref_way ON osm_way_refs (node_id)");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'idx_osm_node_way' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating the OSM "raw" relations */
    strcpy (sql, "CREATE TABLE osm_relations (\n");
    strcat (sql, "rel_id INTEGER NOT NULL PRIMARY KEY)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_relations' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating the OSM "raw" relation tags */
    strcpy (sql, "CREATE TABLE osm_relation_tags (\n");
    strcat (sql, "rel_id INTEGER NOT NULL,\n");
    strcat (sql, "sub INTEGER NOT NULL,\n");
    strcat (sql, "k TEXT,\n");
    strcat (sql, "v TEXT,\n");
    strcat (sql, "CONSTRAINT pk_osm_reltags PRIMARY KEY (rel_id, sub),\n");
    strcat (sql, "CONSTRAINT fk_osm_reltags FOREIGN KEY (rel_id) ");
    strcat (sql, "REFERENCES osm_relations (rel_id))\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_relation_tags' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating the OSM "raw" relation-node refs */
    strcpy (sql, "CREATE TABLE osm_relation_refs (\n");
    strcat (sql, "rel_id INTEGER NOT NULL,\n");
    strcat (sql, "sub INTEGER NOT NULL,\n");
    strcat (sql, "type TEXT NOT NULL,\n");
    strcat (sql, "ref INTEGER NOT NULL,\n");
    strcat (sql, "role TEXT,");
    strcat (sql, "CONSTRAINT pk_osm_relnoderefs PRIMARY KEY (rel_id, sub),\n");
    strcat (sql, "CONSTRAINT fk_osm_relnoderefs FOREIGN KEY (rel_id) ");
    strcat (sql, "REFERENCES osm_relations (rel_id))\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_relation_refs' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
/* creating an index supporting osm_relation_refs.ref */
    strcpy (sql,
	    "CREATE INDEX idx_osm_ref_relation ON osm_relation_refs (type, ref)");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'idx_osm_relation' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    return 1;
}

static void
open_db (const char *path, sqlite3 ** handle, int cache_size, void *cache)
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
	  return;
      }
    spatialite_init_ex (db_handle, cache, 0);
    spatialite_autocreate (db_handle);
    if (cache_size > 0)
      {
	  /* setting the CACHE-SIZE */
	  sprintf (sql, "PRAGMA cache_size=%d", cache_size);
	  sqlite3_exec (db_handle, sql, NULL, NULL, NULL);
      }
    /* enforcing PK/FK constraints */
    sprintf (sql, "PRAGMA foreign_keys=1");
    sqlite3_exec (db_handle, sql, NULL, NULL, NULL);

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
check_bbox (double *minx, double *miny, double *maxx, double *maxy)
{
/* checking the BoundingBox for validity */
    double save;
    int ret = 1;
    if (*minx < -180.0)
      {
	  *minx = -180.0;
	  ret = 0;
      }
    if (*maxx > 180.0)
      {
	  *maxx = 180.0;
	  ret = 0;
      }
    if (*miny < -90.0)
      {
	  *miny = -90.0;
	  ret = 0;
      }
    if (*maxy > 90.0)
      {
	  *maxy = 90.0;
	  ret = 0;
      }
    if (*minx > *maxx)
      {
	  save = *minx;
	  *minx = *maxx;
	  *maxx = save;
	  ret = 0;
      }
    if (*miny > *maxy)
      {
	  save = *miny;
	  *miny = *maxy;
	  *maxy = save;
	  ret = 0;
      }
    return ret;
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_osm_overpass ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr,
	     "-d or --db-path     pathname    the SpatiaLite DB path\n");
    fprintf (stderr,
	     "-minx or --bbox-minx  coord     BoundingBox - west longitude\n");
    fprintf (stderr,
	     "-maxx or --bbox-maxx  coord     BoundingBox - east longitude\n");
    fprintf (stderr,
	     "-miny or --bbox-miny  coord     BoundingBox - south latitude\n");
    fprintf (stderr,
	     "-maxy or --bbox-maxy  coord     BoundingBox - north latitude\n\n");
    fprintf (stderr, "you can specify the following options as well\n");
    fprintf (stderr,
	     "-o or --osm-service   URL       URL of OSM Overpass service:\n");
    fprintf (stderr,
	     "                                http://overpass-api.de/api (default)\n");
    fprintf (stderr,
	     "                                http://overpass.osm.rambler.ru/cgi\n");
    fprintf (stderr,
	     "                                http://api.openstreetmap.fr/oapi\n");
    fprintf (stderr,
	     "-mode or --mode       mode      one of: RAW / MAP (default) / ROAD / RAIL\n");
    fprintf (stderr,
	     "-cs or --cache-size   num       DB cache size (how many pages)\n");
    fprintf (stderr,
	     "-m or --in-memory               using IN-MEMORY database\n");
    fprintf (stderr,
	     "-jo or --journal-off            unsafe [but faster] mode\n");
    fprintf (stderr,
	     "-p or --preserve                skipping final cleanup (preserving OSM tables)\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    sqlite3 *handle;
    int i;
    int next_arg = ARG_NONE;
    const char *osm_url = "http://overpass-api.de/api";
    const char *db_path = NULL;
    int in_memory = 0;
    int cache_size = 0;
    int journal_off = 0;
    int error = 0;
    void *cache;
    double minx;
    int ok_minx = 0;
    double miny;
    int ok_miny = 0;
    double maxx;
    int ok_maxx = 0;
    double maxy;
    int ok_maxy = 0;
    int bbox = 1;
    int mode = MODE_MAP;
    int preserve_osm_tables = 0;
    struct aux_params params;
    struct tiled_download downloader;
    struct download_tile *tile;
    double extent_h;
    double extent_v;
    double step_v;
    double step_h;
    double factor;
    double base_x;
    double base_y;
    double right_x;
    double top_y;

/* initializing the tiled downloader object */
    downloader.first = NULL;
    downloader.last = NULL;
    downloader.count = 0;

/* initializing the aux-structs */
    params.db_handle = NULL;
    params.ins_nodes_stmt = NULL;
    params.ins_node_tags_stmt = NULL;
    params.ins_ways_stmt = NULL;
    params.ins_way_tags_stmt = NULL;
    params.ins_way_refs_stmt = NULL;
    params.ins_relations_stmt = NULL;
    params.ins_relation_tags_stmt = NULL;
    params.ins_relation_refs_stmt = NULL;
    params.wr_nodes = 0;
    params.wr_node_tags = 0;
    params.wr_ways = 0;
    params.wr_way_tags = 0;
    params.wr_way_refs = 0;
    params.wr_relations = 0;
    params.wr_rel_tags = 0;
    params.wr_rel_refs = 0;
    params.osm_url = NULL;

    for (i = 1; i < argc; i++)
      {
	  /* parsing the invocation arguments */
	  if (next_arg != ARG_NONE)
	    {
		switch (next_arg)
		  {
		  case ARG_DB_PATH:
		      db_path = argv[i];
		      break;
		  case ARG_OSM_URL:
		      osm_url = argv[i];
		      break;
		  case ARG_CACHE_SIZE:
		      cache_size = atoi (argv[i]);
		      break;
		  case ARG_MINX:
		      minx = atof (argv[i]);
		      ok_minx = 1;
		      break;
		  case ARG_MINY:
		      miny = atof (argv[i]);
		      ok_miny = 1;
		      break;
		  case ARG_MAXX:
		      maxx = atof (argv[i]);
		      ok_maxx = 1;
		      break;
		  case ARG_MAXY:
		      maxy = atof (argv[i]);
		      ok_maxy = 1;
		      break;
		  case ARG_MODE:
		      if (strcmp (argv[i], "RAW") == 0)
			  mode = MODE_RAW;
		      if (strcmp (argv[i], "MAP") == 0)
			  mode = MODE_MAP;
		      if (strcmp (argv[i], "ROAD") == 0)
			  mode = MODE_ROAD;
		      if (strcmp (argv[i], "RAIL") == 0)
			  mode = MODE_RAIL;
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
	  if (strcasecmp (argv[i], "--osm-service") == 0
	      || strcmp (argv[i], "-o") == 0)
	    {
		next_arg = ARG_OSM_URL;
		continue;
	    }
	  if (strcasecmp (argv[i], "--bbox-minx") == 0
	      || strcmp (argv[i], "-minx") == 0)
	    {
		next_arg = ARG_MINX;
		continue;
	    }
	  if (strcasecmp (argv[i], "--bbox-miny") == 0
	      || strcmp (argv[i], "-miny") == 0)
	    {
		next_arg = ARG_MINY;
		continue;
	    }
	  if (strcasecmp (argv[i], "--bbox-maxx") == 0
	      || strcmp (argv[i], "-maxx") == 0)
	    {
		next_arg = ARG_MAXX;
		continue;
	    }
	  if (strcasecmp (argv[i], "--bbox-maxy") == 0
	      || strcmp (argv[i], "-maxy") == 0)
	    {
		next_arg = ARG_MAXY;
		continue;
	    }
	  if (strcasecmp (argv[i], "--mode") == 0
	      || strcmp (argv[i], "-mode") == 0)
	    {
		next_arg = ARG_MODE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--preserve") == 0
	      || strcmp (argv[i], "-p") == 0)
	    {
		preserve_osm_tables = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--cache-size") == 0
	      || strcmp (argv[i], "-cs") == 0)
	    {
		next_arg = ARG_CACHE_SIZE;
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
	  fprintf (stderr, "unknown argument: %s\n", argv[i]);
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }

/* checking the arguments */
    if (!db_path)
      {
	  fprintf (stderr, "did you forget setting the --db-path argument ?\n");
	  error = 1;
      }
    if (!ok_minx)
      {
	  fprintf (stderr,
		   "did you forget setting the --bbox-minx argument ?\n");
	  error = 1;
	  bbox = 0;
      }
    if (!ok_miny)
      {
	  fprintf (stderr,
		   "did you forget setting the --bbox-miny argument ?\n");
	  error = 1;
	  bbox = 0;
      }
    if (!ok_maxx)
      {
	  fprintf (stderr,
		   "did you forget setting the --bbox-maxx argument ?\n");
	  error = 1;
	  bbox = 0;
      }
    if (!ok_maxy)
      {
	  fprintf (stderr,
		   "did you forget setting the --bbox-maxy argument ?\n");
	  error = 1;
	  bbox = 0;
      }
    if (bbox)
      {
	  if (!check_bbox (&minx, &miny, &maxx, &maxy))
	    {
		fprintf (stderr,
			 "invalid BoundingBox; did you possibly intended\n"
			 "\t-minx %1.6f -miny %1.6f -maxx %1.6f -maxy %1.6f ?\n",
			 minx, miny, maxx, maxy);
		error = 1;
	    }
      }

    if (error)
      {
	  do_help ();
	  return -1;
      }

/* preparing individual download tiles */
    extent_h = maxx - minx;
    extent_v = maxy - miny;
    step_h = extent_h;
    factor = 1.0;
    while (step_h > 0.35)
      {
	  factor += 1.0;
	  step_h = extent_h / factor;
      }
    step_v = extent_v;
    factor = 1.0;
    while (step_v > 0.35)
      {
	  factor += 1.0;
	  step_v = extent_v / factor;
      }
    base_y = miny;
    while (base_y < maxy)
      {
	  top_y = base_y + step_v + 0.01;
	  if (top_y > maxy)
	      top_y = maxy;
	  base_x = minx;
	  while (base_x < maxx)
	    {
		right_x = base_x + step_h + 0.01;
		if (right_x > maxx)
		    right_x = maxx;
		add_download_tile (&downloader, base_x, base_y, right_x, top_y);
		base_x += step_h;
	    }
	  base_y += step_v;
      }

/* opening the DB */
    if (in_memory)
	cache_size = 0;
    cache = spatialite_alloc_connection ();
    open_db (db_path, &handle, cache_size, cache);
    if (!handle)
	return -1;
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
    params.db_handle = handle;
    params.cache = cache;
    params.osm_url = osm_url;
    params.mode = mode;

/* creating the OSM raw tables */
    if (!create_osm_raw_tables (&params))
      {
	  sqlite3_close (handle);
	  return -1;
      }
/* creating the  SQL prepared statements */
    create_sql_stmts (&params, journal_off);

    tile = downloader.first;
    while (tile != NULL)
      {
	  /* downloading and parsing an input OSM dataset (tiled) */
	  if (mode == MODE_ROAD || mode == MODE_RAIL)
	    {
		printf ("Downloading and parsing OSM tile %d of %d\r",
			tile->tile_no, downloader.count);
		if (!osm_parse (&params, tile, 0))
		  {
		      fprintf (stderr,
			       "\noperation aborted due to unrecoverable errors\n\n");
		      finalize_sql_stmts (&params);
		      sqlite3_close (handle);
		      return -1;
		  }
	    }
	  else
	    {
		printf
		    ("Downloading and parsing OSM tile %d of %d - Nodes        \r",
		     tile->tile_no, downloader.count);
		if (!osm_parse (&params, tile, OBJ_NODES))
		  {
		      fprintf (stderr,
			       "\noperation aborted due to unrecoverable errors\n\n");
		      finalize_sql_stmts (&params);
		      sqlite3_close (handle);
		      return -1;
		  }
		printf
		    ("Downloading and parsing OSM tile %d of %d - Ways          \r",
		     tile->tile_no, downloader.count);
		if (!osm_parse (&params, tile, OBJ_WAYS))
		  {
		      fprintf (stderr,
			       "\noperation aborted due to unrecoverable errors\n\n");
		      finalize_sql_stmts (&params);
		      sqlite3_close (handle);
		      return -1;
		  }
		printf
		    ("Downloading and parsing OSM tile %d of %d - Relations     \r",
		     tile->tile_no, downloader.count);
		if (!osm_parse (&params, tile, OBJ_RELATIONS))
		  {
		      fprintf (stderr,
			       "\noperation aborted due to unrecoverable errors\n\n");
		      finalize_sql_stmts (&params);
		      sqlite3_close (handle);
		      return -1;
		  }
	    }
	  tile = tile->next;
      }
    printf ("Download completed                                        \n");

/* finalizing SQL prepared statements */
    finalize_sql_stmts (&params);

/* printing out statistics */
    printf ("inserted %d nodes\n", params.wr_nodes);
    printf ("\t%d tags\n", params.wr_node_tags);
    printf ("inserted %d ways\n", params.wr_ways);
    printf ("\t%d tags\n", params.wr_way_tags);
    printf ("\t%d node-refs\n", params.wr_way_refs);
    printf ("inserted %d relations\n", params.wr_relations);
    printf ("\t%d tags\n", params.wr_rel_tags);
    printf ("\t%d refs\n", params.wr_rel_refs);

    if (mode == MODE_ROAD)
      {
	  /* building the Road Network */
	  int cnt_nodes = 0;
	  int cnt_arcs = 0;
	  if (!create_road_tables (&params))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  if (!populate_road_network (&params, &cnt_nodes, &cnt_arcs))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  if (!create_road_rtrees (&params))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  printf ("inserted %d ROAD nodes\n", cnt_nodes);
	  printf ("inserted %d ROAD arcs\n", cnt_arcs);
	  if (!preserve_osm_tables)
	      do_clean_roads (&params);
      }
    else if (mode == MODE_RAIL)
      {
	  /* building the Rail Network */
	  int cnt_nodes = 0;
	  int cnt_arcs = 0;
	  int cnt_stations = 0;
	  if (!create_rail_tables (&params))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  if (!populate_rail_network
	      (&params, &cnt_nodes, &cnt_arcs, &cnt_stations))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  if (!create_rail_rtrees (&params))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  printf ("inserted %d RAIL nodes\n", cnt_nodes);
	  printf ("inserted %d RAIL arcs\n", cnt_arcs);
	  printf ("inserted %d RAIL stations\n", cnt_stations);
	  if (!preserve_osm_tables)
	      do_clean_rails (&params);
      }
    else if (mode == MODE_MAP)
      {
	  /* building a full MAP */
	  int points = 0;
	  int linestrings = 0;
	  int polygons = 0;
	  int multi_linestrings = 0;
	  int multi_polygons = 0;
	  if (!create_map_indices (&params))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  if (!populate_map_layers
	      (&params, &points, &linestrings, &polygons, &multi_linestrings,
	       &multi_polygons))
	    {
		sqlite3_close (handle);
		return -1;
	    }
	  finalize_map_stmts ();
	  printf ("inserted %d Point Features\n", points);
	  printf ("inserted %d Linestring Features\n", linestrings);
	  printf ("inserted %d Polygon Features\n", polygons);
	  printf ("inserted %d MultiLinestring Features\n", multi_linestrings);
	  printf ("inserted %d MultiPolygon Features\n", multi_polygons);
	  if (!preserve_osm_tables)
	      do_clean_map (&params);
      }

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

/* closing the DB connection */
    sqlite3_close (handle);
    spatialite_cleanup_ex (cache);
    spatialite_shutdown ();
    downloader_cleanup (&downloader);
    return 0;
}
