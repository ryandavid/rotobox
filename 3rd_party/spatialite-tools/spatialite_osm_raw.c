/* 
/ spatialite_osm_raw
/
/ a tool loading "raw" OSM maps into a SpatiaLite DB
/
/ version 1.0, 2010 September 13
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
#define ARG_CACHE_SIZE	3

struct aux_params
{
/* an auxiliary struct used for XML parsing */
    sqlite3 *db_handle;
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
};

static int
insert_node (struct aux_params *params, const readosm_node * node)
{
    int ret;
    unsigned char *blob;
    int blob_size;
    int i_tag;
    const readosm_tag *p_tag;
    gaiaGeomCollPtr geom = NULL;
    if (node->longitude != READOSM_UNDEFINED
	&& node->latitude != READOSM_UNDEFINED)
      {
	  geom = gaiaAllocGeomColl ();
	  geom->Srid = 4326;
	  gaiaAddPointToGeomColl (geom, node->longitude, node->latitude);
      }
    sqlite3_reset (params->ins_nodes_stmt);
    sqlite3_clear_bindings (params->ins_nodes_stmt);
    sqlite3_bind_int64 (params->ins_nodes_stmt, 1, node->id);
    if (node->version == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_nodes_stmt, 2);
    else
	sqlite3_bind_int64 (params->ins_nodes_stmt, 2, node->version);
    if (node->timestamp == NULL)
	sqlite3_bind_null (params->ins_nodes_stmt, 3);
    else
	sqlite3_bind_text (params->ins_nodes_stmt, 3, node->timestamp,
			   strlen (node->timestamp), SQLITE_STATIC);
    if (node->uid == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_nodes_stmt, 4);
    else
	sqlite3_bind_int64 (params->ins_nodes_stmt, 4, node->uid);
    if (node->user == NULL)
	sqlite3_bind_null (params->ins_nodes_stmt, 5);
    else
	sqlite3_bind_text (params->ins_nodes_stmt, 5, node->user,
			   strlen (node->user), SQLITE_STATIC);
    if (node->changeset == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_nodes_stmt, 6);
    else
	sqlite3_bind_int64 (params->ins_nodes_stmt, 6, node->changeset);
    if (!geom)
	sqlite3_bind_null (params->ins_nodes_stmt, 7);
    else
      {
	  gaiaToSpatiaLiteBlobWkb (geom, &blob, &blob_size);
	  gaiaFreeGeomColl (geom);
	  sqlite3_bind_blob (params->ins_nodes_stmt, 7, blob, blob_size, free);
      }
    ret = sqlite3_step (params->ins_nodes_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  fprintf (stderr, "sqlite3_step() error: INSERT INTO osm_nodes\n");
	  return 0;
      }
    params->wr_nodes += 1;

    for (i_tag = 0; i_tag < node->tag_count; i_tag++)
      {
	  p_tag = node->tags + i_tag;
	  sqlite3_reset (params->ins_node_tags_stmt);
	  sqlite3_clear_bindings (params->ins_node_tags_stmt);
	  sqlite3_bind_int64 (params->ins_node_tags_stmt, 1, node->id);
	  sqlite3_bind_int (params->ins_node_tags_stmt, 2, i_tag);
	  if (p_tag->key == NULL)
	      sqlite3_bind_null (params->ins_node_tags_stmt, 3);
	  else
	      sqlite3_bind_text (params->ins_node_tags_stmt, 3, p_tag->key,
				 strlen (p_tag->key), SQLITE_STATIC);
	  if (p_tag->value == NULL)
	      sqlite3_bind_null (params->ins_node_tags_stmt, 4);
	  else
	      sqlite3_bind_text (params->ins_node_tags_stmt, 4, p_tag->value,
				 strlen (p_tag->value), SQLITE_STATIC);
	  ret = sqlite3_step (params->ins_node_tags_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr,
			 "sqlite3_step() error: INSERT INTO osm_node_tags\n");
		return 0;
	    }
	  params->wr_node_tags += 1;
      }
    return 1;
}

static int
insert_way (struct aux_params *params, const readosm_way * way)
{
    int ret;
    int i_tag;
    int i_ref;
    const readosm_tag *p_tag;
    sqlite3_reset (params->ins_ways_stmt);
    sqlite3_clear_bindings (params->ins_ways_stmt);
    sqlite3_bind_int64 (params->ins_ways_stmt, 1, way->id);
    if (way->version == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_ways_stmt, 2);
    else
	sqlite3_bind_int64 (params->ins_ways_stmt, 2, way->version);
    if (way->timestamp == NULL)
	sqlite3_bind_null (params->ins_ways_stmt, 3);
    else
	sqlite3_bind_text (params->ins_ways_stmt, 3, way->timestamp,
			   strlen (way->timestamp), SQLITE_STATIC);
    if (way->uid == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_ways_stmt, 4);
    else
	sqlite3_bind_int64 (params->ins_ways_stmt, 4, way->uid);
    if (way->user == NULL)
	sqlite3_bind_null (params->ins_ways_stmt, 5);
    else
	sqlite3_bind_text (params->ins_ways_stmt, 5, way->user,
			   strlen (way->user), SQLITE_STATIC);
    if (way->changeset == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_ways_stmt, 6);
    else
	sqlite3_bind_int64 (params->ins_ways_stmt, 6, way->changeset);
    ret = sqlite3_step (params->ins_ways_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  fprintf (stderr, "sqlite3_step() error: INSERT INTO osm_ways\n");
	  return 0;
      }
    params->wr_ways += 1;

    for (i_tag = 0; i_tag < way->tag_count; i_tag++)
      {
	  p_tag = way->tags + i_tag;
	  sqlite3_reset (params->ins_way_tags_stmt);
	  sqlite3_clear_bindings (params->ins_way_tags_stmt);
	  sqlite3_bind_int64 (params->ins_way_tags_stmt, 1, way->id);
	  sqlite3_bind_int (params->ins_way_tags_stmt, 2, i_tag);
	  if (p_tag->key == NULL)
	      sqlite3_bind_null (params->ins_way_tags_stmt, 3);
	  else
	      sqlite3_bind_text (params->ins_way_tags_stmt, 3, p_tag->key,
				 strlen (p_tag->key), SQLITE_STATIC);
	  if (p_tag->value == NULL)
	      sqlite3_bind_null (params->ins_way_tags_stmt, 4);
	  else
	      sqlite3_bind_text (params->ins_way_tags_stmt, 4, p_tag->value,
				 strlen (p_tag->value), SQLITE_STATIC);
	  ret = sqlite3_step (params->ins_way_tags_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr,
			 "sqlite3_step() error: INSERT INTO osm_way_tags\n");
		return 0;
	    }
	  params->wr_way_tags += 1;
      }

    for (i_ref = 0; i_ref < way->node_ref_count; i_ref++)
      {
	  sqlite3_int64 node_id = *(way->node_refs + i_ref);
	  sqlite3_reset (params->ins_way_refs_stmt);
	  sqlite3_clear_bindings (params->ins_way_refs_stmt);
	  sqlite3_bind_int64 (params->ins_way_refs_stmt, 1, way->id);
	  sqlite3_bind_int (params->ins_way_refs_stmt, 2, i_ref);
	  sqlite3_bind_int64 (params->ins_way_refs_stmt, 3, node_id);
	  ret = sqlite3_step (params->ins_way_refs_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr,
			 "sqlite3_step() error: INSERT INTO osm_way_refs\n");
		return 0;
	    }
	  params->wr_way_refs += 1;
      }
    return 1;
}

static int
insert_relation (struct aux_params *params, const readosm_relation * relation)
{
    int ret;
    int i_tag;
    int i_member;
    const readosm_tag *p_tag;
    const readosm_member *p_member;
    sqlite3_reset (params->ins_relations_stmt);
    sqlite3_clear_bindings (params->ins_relations_stmt);
    sqlite3_bind_int64 (params->ins_relations_stmt, 1, relation->id);
    if (relation->version == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_relations_stmt, 2);
    else
	sqlite3_bind_int64 (params->ins_relations_stmt, 2, relation->version);
    if (relation->timestamp == NULL)
	sqlite3_bind_null (params->ins_relations_stmt, 3);
    else
	sqlite3_bind_text (params->ins_relations_stmt, 3, relation->timestamp,
			   strlen (relation->timestamp), SQLITE_STATIC);
    if (relation->uid == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_relations_stmt, 4);
    else
	sqlite3_bind_int64 (params->ins_relations_stmt, 4, relation->uid);
    if (relation->user == NULL)
	sqlite3_bind_null (params->ins_relations_stmt, 5);
    else
	sqlite3_bind_text (params->ins_relations_stmt, 5, relation->user,
			   strlen (relation->user), SQLITE_STATIC);
    if (relation->changeset == READOSM_UNDEFINED)
	sqlite3_bind_null (params->ins_relations_stmt, 6);
    else
	sqlite3_bind_int64 (params->ins_relations_stmt, 6, relation->changeset);
    ret = sqlite3_step (params->ins_relations_stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  fprintf (stderr, "sqlite3_step() error: INSERT INTO osm_relations\n");
	  return 0;
      }
    params->wr_relations += 1;

    for (i_tag = 0; i_tag < relation->tag_count; i_tag++)
      {
	  p_tag = relation->tags + i_tag;
	  sqlite3_reset (params->ins_relation_tags_stmt);
	  sqlite3_clear_bindings (params->ins_relation_tags_stmt);
	  sqlite3_bind_int64 (params->ins_relation_tags_stmt, 1, relation->id);
	  sqlite3_bind_int (params->ins_relation_tags_stmt, 2, i_tag);
	  if (p_tag->key == NULL)
	      sqlite3_bind_null (params->ins_relation_tags_stmt, 3);
	  else
	      sqlite3_bind_text (params->ins_relation_tags_stmt, 3, p_tag->key,
				 strlen (p_tag->key), SQLITE_STATIC);
	  if (p_tag->value == NULL)
	      sqlite3_bind_null (params->ins_relation_tags_stmt, 4);
	  else
	      sqlite3_bind_text (params->ins_relation_tags_stmt, 4,
				 p_tag->value, strlen (p_tag->value),
				 SQLITE_STATIC);
	  ret = sqlite3_step (params->ins_relation_tags_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr,
			 "sqlite3_step() error: INSERT INTO osm_relation_tags\n");
		return 0;
	    }
	  params->wr_rel_tags += 1;
      }

    for (i_member = 0; i_member < relation->member_count; i_member++)
      {
	  p_member = relation->members + i_member;
	  sqlite3_reset (params->ins_relation_refs_stmt);
	  sqlite3_clear_bindings (params->ins_relation_refs_stmt);
	  sqlite3_bind_int64 (params->ins_relation_refs_stmt, 1, relation->id);
	  sqlite3_bind_int (params->ins_relation_refs_stmt, 2, i_member);
	  if (p_member->member_type == READOSM_MEMBER_NODE)
	      sqlite3_bind_text (params->ins_relation_refs_stmt, 3, "N", 1,
				 SQLITE_STATIC);
	  else if (p_member->member_type == READOSM_MEMBER_WAY)
	      sqlite3_bind_text (params->ins_relation_refs_stmt, 3, "W", 1,
				 SQLITE_STATIC);
	  else if (p_member->member_type == READOSM_MEMBER_RELATION)
	      sqlite3_bind_text (params->ins_relation_refs_stmt, 3, "R", 1,
				 SQLITE_STATIC);
	  else
	      sqlite3_bind_text (params->ins_relation_refs_stmt, 3, "?", 1,
				 SQLITE_STATIC);
	  sqlite3_bind_int64 (params->ins_relation_refs_stmt, 4, p_member->id);
	  if (p_member->role == NULL)
	      sqlite3_bind_null (params->ins_relation_refs_stmt, 5);
	  else
	      sqlite3_bind_text (params->ins_relation_refs_stmt, 5,
				 p_member->role, strlen (p_member->role),
				 SQLITE_STATIC);
	  ret = sqlite3_step (params->ins_relation_refs_stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr,
			 "sqlite3_step() error: INSERT INTO osm_relation_refs\n");
		return 0;
	    }
	  params->wr_rel_refs += 1;
      }
    return 1;
}

static int
consume_node (const void *user_data, const readosm_node * node)
{
/* processing an OSM Node (ReadOSM callback function) */
    struct aux_params *params = (struct aux_params *) user_data;
    if (!insert_node (params, node))
	return READOSM_ABORT;
    return READOSM_OK;
}

static int
consume_way (const void *user_data, const readosm_way * way)
{
/* processing an OSM Way (ReadOSM callback function) */
    struct aux_params *params = (struct aux_params *) user_data;
    if (!insert_way (params, way))
	return READOSM_ABORT;
    return READOSM_OK;
}

static int
consume_relation (const void *user_data, const readosm_relation * relation)
{
/* processing an OSM Relation (ReadOSM callback function) */
    struct aux_params *params = (struct aux_params *) user_data;
    if (!insert_relation (params, relation))
	return READOSM_ABORT;
    return READOSM_OK;
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
    strcpy (sql,
	    "INSERT INTO osm_nodes (node_id, version, timestamp, uid, user, changeset, filtered, Geometry) ");
    strcat (sql, "VALUES (?, ?, ?, ?, ?, ?, 0, ?)");
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
    strcpy (sql,
	    "INSERT INTO osm_ways (way_id, version, timestamp, uid, user, changeset, filtered) ");
    strcat (sql, "VALUES (?, ?, ?, ?, ?, ?, 0)");
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
    strcpy (sql,
	    "INSERT INTO osm_relations (rel_id, version, timestamp, uid, user, changeset, filtered) ");
    strcat (sql, "VALUES (?, ?, ?, ?, ?, ?, 0)");
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

static void
open_db (const char *path, sqlite3 ** handle, int cache_size, void *cache)
{
/* opening the DB */
    sqlite3 *db_handle;
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

/* creating the OSM "raw" nodes */
    strcpy (sql, "CREATE TABLE osm_nodes (\n");
    strcat (sql, "node_id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "version INTEGER,\n");
    strcat (sql, "timestamp TEXT,\n");
    strcat (sql, "uid INTEGER,\n");
    strcat (sql, "user TEXT,\n");
    strcat (sql, "changeset INTEGER,\n");
    strcat (sql, "filtered INTEGER NOT NULL)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (db_handle);
	  return;
      }
    strcpy (sql,
	    "SELECT AddGeometryColumn('osm_nodes', 'Geometry', 4326, 'POINT', 'XY')");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_nodes' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (db_handle);
	  return;
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
	  sqlite3_close (db_handle);
	  return;
      }
/* creating the OSM "raw" ways */
    strcpy (sql, "CREATE TABLE osm_ways (\n");
    strcat (sql, "way_id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "version INTEGER,\n");
    strcat (sql, "timestamp TEXT,\n");
    strcat (sql, "uid INTEGER,\n");
    strcat (sql, "user TEXT,\n");
    strcat (sql, "changeset INTEGER,\n");
    strcat (sql, "filtered INTEGER NOT NULL)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_ways' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (db_handle);
	  return;
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
	  sqlite3_close (db_handle);
	  return;
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
	  sqlite3_close (db_handle);
	  return;
      }
/* creating an index supporting osm_way_refs.node_id */
    strcpy (sql, "CREATE INDEX idx_osm_ref_way ON osm_way_refs (node_id)");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE INDEX 'idx_osm_node_way' error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (db_handle);
	  return;
      }
/* creating the OSM "raw" relations */
    strcpy (sql, "CREATE TABLE osm_relations (\n");
    strcat (sql, "rel_id INTEGER NOT NULL PRIMARY KEY,\n");
    strcat (sql, "version INTEGER,\n");
    strcat (sql, "timestamp TEXT,\n");
    strcat (sql, "uid INTEGER,\n");
    strcat (sql, "user TEXT,\n");
    strcat (sql, "changeset INTEGER,\n");
    strcat (sql, "filtered INTEGER NOT NULL)\n");
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE 'osm_relations' error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  sqlite3_close (db_handle);
	  return;
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
	  sqlite3_close (db_handle);
	  return;
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
	  sqlite3_close (db_handle);
	  return;
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
	  sqlite3_close (db_handle);
	  return;
      }

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

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_osm_raw ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr, "-o or --osm-path pathname       the OSM-file path\n");
    fprintf (stderr,
	     "                 both OSM-XML (*.osm) and OSM-ProtoBuf\n");
    fprintf (stderr,
	     "                 (*.osm.pbf) are indifferently supported.\n\n");
    fprintf (stderr,
	     "-d or --db-path  pathname       the SpatiaLite DB path\n\n");
    fprintf (stderr, "you can specify the following options as well\n");
    fprintf (stderr,
	     "-cs or --cache-size    num      DB cache size (how many pages)\n");
    fprintf (stderr,
	     "-m or --in-memory               using IN-MEMORY database\n");
    fprintf (stderr,
	     "-jo or --journal-off            unsafe [but faster] mode\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    sqlite3 *handle;
    int i;
    int next_arg = ARG_NONE;
    const char *osm_path = NULL;
    const char *db_path = NULL;
    int in_memory = 0;
    int cache_size = 0;
    int journal_off = 0;
    int error = 0;
    struct aux_params params;
    const void *osm_handle;
    void *cache;

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

    if (error)
      {
	  do_help ();
	  return -1;
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

/* creating SQL prepared statements */
    create_sql_stmts (&params, journal_off);

/* parsing the input OSM-file */
    if (readosm_open (osm_path, &osm_handle) != READOSM_OK)
      {
	  fprintf (stderr, "cannot open %s\n", osm_path);
	  finalize_sql_stmts (&params);
	  sqlite3_close (handle);
	  readosm_close (osm_handle);
	  return -1;
      }
    if (readosm_parse
	(osm_handle, &params, consume_node, consume_way,
	 consume_relation) != READOSM_OK)
      {
	  fprintf (stderr, "unrecoverable error while parsing %s\n", osm_path);
	  finalize_sql_stmts (&params);
	  sqlite3_close (handle);
	  readosm_close (osm_handle);
	  return -1;
      }
    readosm_close (osm_handle);

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
    return 0;
}
