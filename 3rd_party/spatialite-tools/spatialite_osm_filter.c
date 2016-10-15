/* 
/ spatialite_osm_filter
/
/ a tool loading OSM-XML maps into a SpatiaLite DB
/
/ version 1.0, 2011 October 31
/
/ Author: Sandro Furieri a.furieri@lqt.it
/
/ Copyright (C) 2011  Alessandro Furieri
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


#define ARG_NONE		0
#define ARG_OSM_PATH	1
#define ARG_DB_PATH		2
#define ARG_CACHE_SIZE	3
#define ARG_MASK_PATH	4

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp	_stricmp
#endif /* not WIN32 */

static char *
clean_xml (const char *in)
{
/* well formatting XML text strings */
    int len = 0;
    char *buf;
    char *p_o;
    const char *p_i = in;
    while (*p_i != '\0')
      {
	  /* calculating the output len */
	  if (*p_i == '"')
	    {
		len += 6;
		p_i++;
		continue;
	    }
	  if (*p_i == '\'')
	    {
		len += 6;
		p_i++;
		continue;
	    }
	  if (*p_i == '&')
	    {
		len += 5;
		p_i++;
		continue;
	    }
	  if (*p_i == '<')
	    {
		len += 4;
		p_i++;
		continue;
	    }
	  if (*p_i == '>')
	    {
		len += 4;
		p_i++;
		continue;
	    }
	  len++;
	  p_i++;
      }

    buf = malloc (len + 1);
    p_o = buf;
    p_i = in;
    while (*p_i != '\0')
      {
	  if (*p_i == '"')
	    {
		*p_o++ = '&';
		*p_o++ = 'q';
		*p_o++ = 'u';
		*p_o++ = 'o';
		*p_o++ = 't';
		*p_o++ = ';';
		p_i++;
		continue;
	    }
	  if (*p_i == '\'')
	    {
		*p_o++ = '&';
		*p_o++ = 'a';
		*p_o++ = 'p';
		*p_o++ = 'o';
		*p_o++ = 's';
		*p_o++ = ';';
		p_i++;
		continue;
	    }
	  if (*p_i == '&')
	    {
		*p_o++ = '&';
		*p_o++ = 'a';
		*p_o++ = 'm';
		*p_o++ = 'p';
		*p_o++ = ';';
		p_i++;
		continue;
	    }
	  if (*p_i == '<')
	    {
		*p_o++ = '&';
		*p_o++ = 'l';
		*p_o++ = 't';
		*p_o++ = ';';
		p_i++;
		continue;
	    }
	  if (*p_i == '>')
	    {
		*p_o++ = '&';
		*p_o++ = 'g';
		*p_o++ = 't';
		*p_o++ = ';';
		p_i++;
		continue;
	    }
	  *p_o++ = *p_i++;
      }
    *p_o = '\0';
    return buf;
}

static int
do_output_nodes (FILE * out, sqlite3 * handle)
{
/* exporting any OSM node */
    char sql[1024];
    int ret;
    int first;
    int close_node;
    sqlite3_stmt *node_query = NULL;
    sqlite3_stmt *query = NULL;

/* preparing the QUERY filtered-NODES statement */
    strcpy (sql, "SELECT node_id FROM osm_nodes WHERE filtered = 1");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &node_query, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  return 0;
      }

/* preparing the QUERY NODES statement */
    strcpy (sql, "SELECT n.node_id, n.version, n.timestamp, ");
    strcat (sql, "n.uid, n.user, n.changeset, ST_X(n.Geometry), ");
    strcat (sql, "ST_Y(n.Geometry), t.k, t.v ");
    strcat (sql, "FROM osm_nodes AS n ");
    strcat (sql, "LEFT JOIN osm_node_tags AS t ON (t.node_id = n.node_id) ");
    strcat (sql, "WHERE n.node_id = ? ");
    strcat (sql, "ORDER BY n.node_id, t.sub");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto stop;
      }

    while (1)
      {
	  /* scrolling the result set */
	  ret = sqlite3_step (node_query);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		sqlite3_int64 id = sqlite3_column_int64 (node_query, 0);

		sqlite3_reset (query);
		sqlite3_clear_bindings (query);
		sqlite3_bind_int64 (query, 1, id);
		first = 1;
		close_node = 0;
		while (1)
		  {
		      /* scrolling the result set */
		      ret = sqlite3_step (query);
		      if (ret == SQLITE_DONE)
			{
			    /* there are no more rows to fetch - we can stop looping */
			    break;
			}
		      if (ret == SQLITE_ROW)
			{
			    /* ok, we've just fetched a valid row */
			    sqlite3_int64 id = sqlite3_column_int64 (query, 0);
			    int version = sqlite3_column_int (query, 1);
			    const char *p_timestamp =
				(const char *) sqlite3_column_text (query, 2);
			    int uid = sqlite3_column_int (query, 3);
			    const char *p_user =
				(const char *) sqlite3_column_text (query, 4);
			    const char *p_changeset =
				(const char *) sqlite3_column_text (query, 5);
			    double x = sqlite3_column_double (query, 6);
			    double y = sqlite3_column_double (query, 7);
			    char *k = NULL;
			    char *v = NULL;
			    if (sqlite3_column_type (query, 8) != SQLITE_NULL)
				k = clean_xml ((const char *)
					       sqlite3_column_text (query, 8));
			    if (sqlite3_column_type (query, 9) != SQLITE_NULL)
				v = clean_xml ((const char *)
					       sqlite3_column_text (query, 9));

			    if (first)
			      {
				  /* first NODE row */
				  char *timestamp = clean_xml (p_timestamp);
				  char *changeset = clean_xml (p_changeset);
				  char *user = NULL;
				  if (p_user)
				      user = clean_xml (p_user);
				  first = 0;
#if defined(_WIN32) || defined(__MINGW32__)
/* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
				  fprintf (out, "\t<node id=\"%I64d\"", id);
#else
				  fprintf (out, "\t<node id=\"%lld\"", id);
#endif
				  if (!user)
				      fprintf (out,
					       " lat=\"%1.7f\" lon=\"%1.7f\" version=\"%d\" changeset=\"%s\" uid=\"%d\" timestamp=\"%s\"",
					       y, x, version, changeset, uid,
					       timestamp);
				  else
				      fprintf (out,
					       " lat=\"%1.7f\" lon=\"%1.7f\" version=\"%d\" changeset=\"%s\" user=\"%s\" uid=\"%d\" timestamp=\"%s\"",
					       y, x, version, changeset, user,
					       uid, timestamp);
				  free (changeset);
				  if (user)
				      free (user);
				  free (timestamp);
				  if (k == NULL && v == NULL)
				      fprintf (out, "/>\n");
				  else
				      fprintf (out, ">\n");
			      }
			    if (k != NULL && v != NULL)
			      {
				  /* NODE tag */
				  fprintf (out,
					   "\t\t<tag k=\"%s\" v=\"%s\"/>\n", k,
					   v);
				  close_node = 1;
			      }
			    if (k)
				free (k);
			    if (v)
				free (v);
			}
		      else
			{
			    /* some unexpected error occurred */
			    fprintf (stderr, "sqlite3_step() error: %s\n",
				     sqlite3_errmsg (handle));
			    goto stop;
			}
		  }
		if (close_node)
		    fprintf (out, "\t</node>\n");
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (handle));
		goto stop;
	    }
      }
    sqlite3_finalize (node_query);
    sqlite3_finalize (query);

    return 1;

  stop:
    sqlite3_finalize (node_query);
    if (query)
	sqlite3_finalize (query);
    return 0;
}

static int
do_output_ways (FILE * out, sqlite3 * handle)
{
/* exporting any OSM way */
    char sql[1024];
    int ret;
    int first;
    sqlite3_stmt *way_query = NULL;
    sqlite3_stmt *query = NULL;
    sqlite3_stmt *query_tag = NULL;

/* preparing the QUERY filtered-WAYS statement */
    strcpy (sql, "SELECT way_id FROM osm_ways WHERE filtered = 1");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &way_query, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  return 0;
      }

/* preparing the QUERY WAY/NODES statement */
    strcpy (sql, "SELECT w.way_id, w.version, w.timestamp, w.uid, ");
    strcat (sql, "w.user, w.changeset, n.node_id ");
    strcat (sql, "FROM osm_ways AS w ");
    strcat (sql, "JOIN osm_way_refs AS n ON (n.way_id = w.way_id) ");
    strcat (sql, "WHERE w.way_id = ? ");
    strcat (sql, "ORDER BY w.way_id, n.sub");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto stop;
      }

/* preparing the QUERY WAY/TAGS statement */
    strcpy (sql, "SELECT t.k, t.v ");
    strcat (sql, "FROM osm_ways AS w ");
    strcat (sql, "JOIN osm_way_tags AS t ON (t.way_id = w.way_id) ");
    strcat (sql, "WHERE w.way_id = ? ");
    strcat (sql, "ORDER BY w.way_id, t.sub");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query_tag, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto stop;
      }

    while (1)
      {
	  /* scrolling the result set */
	  ret = sqlite3_step (way_query);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		sqlite3_int64 id = sqlite3_column_int64 (way_query, 0);

		sqlite3_reset (query);
		sqlite3_clear_bindings (query);
		sqlite3_bind_int64 (query, 1, id);
		first = 1;
		while (1)
		  {
		      /* scrolling the result set */
		      ret = sqlite3_step (query);
		      if (ret == SQLITE_DONE)
			{
			    /* there are no more rows to fetch - we can stop looping */
			    break;
			}
		      if (ret == SQLITE_ROW)
			{
			    /* ok, we've just fetched a valid row */
			    sqlite3_int64 id = sqlite3_column_int64 (query, 0);
			    int version = sqlite3_column_int (query, 1);
			    const char *p_timestamp =
				(const char *) sqlite3_column_text (query, 2);
			    int uid = sqlite3_column_int (query, 3);
			    const char *p_user =
				(const char *) sqlite3_column_text (query, 4);
			    const char *p_changeset =
				(const char *) sqlite3_column_text (query, 5);
			    sqlite3_int64 node_id =
				sqlite3_column_int64 (query, 6);

			    if (first)
			      {
				  /* first WAY row */
				  char *timestamp = clean_xml (p_timestamp);
				  char *changeset = clean_xml (p_changeset);
				  char *user = NULL;
				  if (p_user)
				      user = clean_xml (p_user);
				  first = 0;
#if defined(_WIN32) || defined(__MINGW32__)
/* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
				  fprintf (out, "\t<way id=\"%I64d\"", id);
#else
				  fprintf (out, "\t<way id=\"%lld\"", id);
#endif
				  if (!user)
				      fprintf (out,
					       " version=\"%d\" changeset=\"%s\" uid=\"%d\" timestamp=\"%s\">\n",
					       version, changeset, uid,
					       timestamp);
				  else
				      fprintf (out,
					       " version=\"%d\" changeset=\"%s\" user=\"%s\" uid=\"%d\" timestamp=\"%s\">\n",
					       version, changeset, user, uid,
					       timestamp);
				  free (changeset);
				  if (user)
				      free (user);
				  free (timestamp);
			      }
			    /* NODE REF tag */
#if defined(_WIN32) || defined(__MINGW32__)
/* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
			    fprintf (out, "\t\t<nd ref=\"%I64d\"/>\n", node_id);
#else
			    fprintf (out, "\t\t<nd ref=\"%lld\"/>\n", node_id);
#endif
			}
		      else
			{
			    /* some unexpected error occurred */
			    fprintf (stderr, "sqlite3_step() error: %s\n",
				     sqlite3_errmsg (handle));
			    goto stop;
			}
		  }
		if (!first)
		  {
		      /* exporting WAY tags */
		      sqlite3_reset (query_tag);
		      sqlite3_clear_bindings (query_tag);
		      sqlite3_bind_int64 (query_tag, 1, id);
		      while (1)
			{
			    /* scrolling the result set */
			    ret = sqlite3_step (query_tag);
			    if (ret == SQLITE_DONE)
			      {
				  /* there are no more rows to fetch - we can stop looping */
				  break;
			      }
			    if (ret == SQLITE_ROW)
			      {
				  /* ok, we've just fetched a valid row */
				  char *k =
				      clean_xml ((const char *)
						 sqlite3_column_text (query_tag,
								      0));
				  char *v =
				      clean_xml ((const char *)
						 sqlite3_column_text (query_tag,
								      1));
				  fprintf (out,
					   "\t\t<tag k=\"%s\" v=\"%s\"/>\n", k,
					   v);
				  free (k);
				  free (v);
			      }
			    else
			      {
				  /* some unexpected error occurred */
				  fprintf (stderr, "sqlite3_step() error: %s\n",
					   sqlite3_errmsg (handle));
				  goto stop;
			      }
			}
		  }
		fprintf (out, "\t</way>\n");
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (handle));
		goto stop;
	    }
      }
    sqlite3_finalize (way_query);
    sqlite3_finalize (query);
    sqlite3_finalize (query_tag);

    return 1;

  stop:
    sqlite3_finalize (way_query);
    if (query)
	sqlite3_finalize (query);
    if (query_tag)
	sqlite3_finalize (query_tag);
    return 0;
}

static int
do_output_relations (FILE * out, sqlite3 * handle)
{
/* exporting any OSM relation */
    char sql[1024];
    int ret;
    int first;
    sqlite3_stmt *rel_query = NULL;
    sqlite3_stmt *query_nd = NULL;
    sqlite3_stmt *query_way = NULL;
    sqlite3_stmt *query_rel = NULL;
    sqlite3_stmt *query_tag = NULL;

/* preparing the QUERY filtered-RELATIONS statement */
    strcpy (sql, "SELECT rel_id FROM osm_relations WHERE filtered = 1");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &rel_query, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  return 0;
      }

/* preparing the QUERY RELATION/NODES statement */
    strcpy (sql, "SELECT r.rel_id, r.version, r.timestamp, r.uid, ");
    strcat (sql, "r.user, r.changeset, n.role, n.ref ");
    strcat (sql, "FROM osm_relations AS r ");
    strcat (sql,
	    "JOIN osm_relation_refs AS n ON (n.type = 'N' AND n.rel_id = r.rel_id) ");
    strcat (sql, "WHERE r.rel_id = ? ");
    strcat (sql, "ORDER BY r.rel_id, n.sub");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query_nd, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto stop;
      }

/* preparing the QUERY RELATION/WAYS statement */
    strcpy (sql, "SELECT r.rel_id, r.version, r.timestamp, r.uid, ");
    strcat (sql, "r.user, r.changeset, w.role, w.ref ");
    strcat (sql, "FROM osm_relations AS r ");
    strcat (sql,
	    "JOIN osm_relation_refs AS w ON (w.type = 'W' AND w.rel_id = r.rel_id) ");
    strcat (sql, "WHERE r.rel_id = ? ");
    strcat (sql, "ORDER BY r.rel_id, w.sub");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query_way, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto stop;
      }

/* preparing the QUERY RELATION/RELATIONS statement */
    strcpy (sql, "SELECT r.rel_id, r.version, r.timestamp, r.uid, ");
    strcat (sql, "r.user, r.changeset, x.role, x.ref ");
    strcat (sql, "FROM osm_relations AS r ");
    strcat (sql,
	    "JOIN osm_relation_refs AS x ON (x.type = 'R' AND x.rel_id = r.rel_id) ");
    strcat (sql, "WHERE r.rel_id = ? ");
    strcat (sql, "ORDER BY r.rel_id, x.sub");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query_rel, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto stop;
      }

/* preparing the QUERY RELATION/TAGS statement */
    strcpy (sql, "SELECT t.k, t.v ");
    strcat (sql, "FROM osm_relations AS r ");
    strcat (sql, "JOIN osm_relation_tags AS t ON (t.rel_id = r.rel_id) ");
    strcat (sql, "WHERE r.rel_id = ? ");
    strcat (sql, "ORDER BY r.rel_id, t.sub");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query_tag, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  goto stop;
      }

    while (1)
      {
	  /* scrolling the result set */
	  ret = sqlite3_step (rel_query);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		sqlite3_int64 id = sqlite3_column_int64 (rel_query, 0);

		sqlite3_reset (query_way);
		sqlite3_clear_bindings (query_way);
		sqlite3_bind_int64 (query_way, 1, id);
		first = 1;
		while (1)
		  {
		      /* scrolling the result set */
		      ret = sqlite3_step (query_way);
		      if (ret == SQLITE_DONE)
			{
			    /* there are no more rows to fetch - we can stop looping */
			    break;
			}
		      if (ret == SQLITE_ROW)
			{
			    /* ok, we've just fetched a valid row */
			    sqlite3_int64 id =
				sqlite3_column_int64 (query_way, 0);
			    int version = sqlite3_column_int (query_way, 1);
			    const char *p_timestamp =
				(const char *) sqlite3_column_text (query_way,
								    2);
			    int uid = sqlite3_column_int (query_way, 3);
			    const char *p_user =
				(const char *) sqlite3_column_text (query_way,
								    4);
			    const char *p_changeset =
				(const char *) sqlite3_column_text (query_way,
								    5);
			    char *role =
				clean_xml ((const char *)
					   sqlite3_column_text (query_way, 6));
			    sqlite3_int64 way_id =
				sqlite3_column_int64 (query_way, 7);

			    if (first)
			      {
				  /* first WAY row */
				  char *timestamp = clean_xml (p_timestamp);
				  char *changeset = clean_xml (p_changeset);
				  char *user = NULL;
				  if (p_user)
				      user = clean_xml (p_user);
				  first = 0;
#if defined(_WIN32) || defined(__MINGW32__)
/* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
				  fprintf (out, "\t<relation id=\"%I64d\"", id);
#else
				  fprintf (out, "\t<relation id=\"%lld\"", id);
#endif
				  if (!user)
				      fprintf (out,
					       " version=\"%d\" changeset=\"%s\" uid=\"%d\" timestamp=\"%s\">\n",
					       version, changeset, uid,
					       timestamp);
				  else
				      fprintf (out,
					       " version=\"%d\" changeset=\"%s\" user=\"%s\" uid=\"%d\" timestamp=\"%s\">\n",
					       version, changeset, user, uid,
					       timestamp);
				  free (changeset);
				  free (user);
				  free (timestamp);
			      }
			    /* NODE REF tag */
#if defined(_WIN32) || defined(__MINGW32__)
/* CAVEAT - M$ runtime doesn't supports %lld for 64 bits */
			    fprintf (out,
				     "\t\t<member type=\"way\" ref=\"%I64d\" role=\"%s\"/>\n",
				     way_id, role);
#else
			    fprintf (out,
				     "\t\t<member type = \"way\" ref=\"%lld\" role=\"%s\"/>\n",
				     way_id, role);
#endif
			    free (role);
			}
		      else
			{
			    /* some unexpected error occurred */
			    fprintf (stderr, "sqlite3_step() error: %s\n",
				     sqlite3_errmsg (handle));
			    goto stop;
			}
		  }
		if (!first)
		  {
		      /* exporting WAY tags */
		      sqlite3_reset (query_tag);
		      sqlite3_clear_bindings (query_tag);
		      sqlite3_bind_int64 (query_tag, 1, id);
		      while (1)
			{
			    /* scrolling the result set */
			    ret = sqlite3_step (query_tag);
			    if (ret == SQLITE_DONE)
			      {
				  /* there are no more rows to fetch - we can stop looping */
				  break;
			      }
			    if (ret == SQLITE_ROW)
			      {
				  /* ok, we've just fetched a valid row */
				  char *k =
				      clean_xml ((const char *)
						 sqlite3_column_text (query_tag,
								      0));
				  char *v =
				      clean_xml ((const char *)
						 sqlite3_column_text (query_tag,
								      1));
				  fprintf (out,
					   "\t\t<tag k=\"%s\" v=\"%s\"/>\n", k,
					   v);
				  free (k);
				  free (v);
			      }
			    else
			      {
				  /* some unexpected error occurred */
				  fprintf (stderr, "sqlite3_step() error: %s\n",
					   sqlite3_errmsg (handle));
				  goto stop;
			      }
			}
		  }
		fprintf (out, "\t</relation>\n");
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (handle));
		goto stop;
	    }
      }
    sqlite3_finalize (rel_query);
    sqlite3_finalize (query_nd);
    sqlite3_finalize (query_way);
    sqlite3_finalize (query_rel);
    sqlite3_finalize (query_tag);

    return 1;

  stop:
    sqlite3_finalize (rel_query);
    if (query_nd)
	sqlite3_finalize (query_nd);
    if (query_way)
	sqlite3_finalize (query_way);
    if (query_rel)
	sqlite3_finalize (query_rel);
    if (query_tag)
	sqlite3_finalize (query_tag);
    return 0;
}

static int
reset_filtered (sqlite3 * handle)
{
/* resetting NODES, WAYS and RELATIONS */
    char sql[1024];
    int ret;
    char *sql_err = NULL;

/* disabling transactions */
    strcpy (sql, "PRAGMA journal_mode=OFF");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA journal_mode=OFF: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* resetting NODES */
    strcpy (sql, "UPDATE osm_nodes SET filtered = 0");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "RESET osm_nodes error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* resetting WAYS */
    strcpy (sql, "UPDATE osm_ways SET filtered = 0");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "RESET osm_ways error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* resetting RELATIONS */
    strcpy (sql, "UPDATE osm_relations SET filtered = 0");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "RESET osm_relations error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* enabling again transactions */
    strcpy (sql, "PRAGMA journal_mode=DELETE");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA journal_mode=DELETE: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

    return 1;
}

static int
filter_rel_relations (sqlite3 * handle)
{
/* selecting any RELATION required by other selected RELATIONS */
    char sql[1024];
    int ret;
    char *sql_err = NULL;

    strcpy (sql, "UPDATE osm_relations SET filtered = 1 ");
    strcat (sql, "WHERE rel_id IN (");
    strcat (sql, "SELECT x.ref ");
    strcat (sql, "FROM osm_relations AS r ");
    strcat (sql,
	    "JOIN osm_relation_refs AS x ON (x.type = 'R' AND r.rel_id = x.rel_id) ");
    strcat (sql, "WHERE r.filtered = 1)");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE osm_relations error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }
    return 1;
}

static int
filter_way_relations (sqlite3 * handle)
{
/* selecting any WAY required by selected RELATIONS */
    char sql[1024];
    int ret;
    char *sql_err = NULL;

    strcpy (sql, "UPDATE osm_ways SET filtered = 1 ");
    strcat (sql, "WHERE way_id IN ( ");
    strcat (sql, "SELECT w.ref ");
    strcat (sql, "FROM osm_relations AS r ");
    strcat (sql,
	    "JOIN osm_relation_refs AS w ON (w.type = 'W' AND r.rel_id = w.rel_id) ");
    strcat (sql, "WHERE r.filtered = 1)");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE osm_ways error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }
    return 1;
}

static int
filter_node_relations (sqlite3 * handle)
{
/* selecting any NODE required by selected RELATIONS */
    char sql[1024];
    int ret;
    char *sql_err = NULL;

    strcpy (sql, "UPDATE osm_nodes SET filtered = 1 ");
    strcat (sql, "WHERE node_id IN ( ");
    strcat (sql, "SELECT n.ref ");
    strcat (sql, "FROM osm_relations AS r ");
    strcat (sql,
	    "JOIN osm_relation_refs AS n ON (n.type = 'N' AND r.rel_id = n.rel_id) ");
    strcat (sql, "WHERE r.filtered = 1)");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE osm_nodes error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }
    return 1;
}

static int
filter_node_ways (sqlite3 * handle)
{
/* selecting any NODE required by selected WAYS */
    char sql[1024];
    int ret;
    char *sql_err = NULL;

    strcpy (sql, "UPDATE osm_nodes SET filtered = 1 ");
    strcat (sql, "WHERE node_id IN ( SELECT n.node_id ");
    strcat (sql, "FROM osm_ways AS w ");
    strcat (sql, "JOIN osm_way_refs AS n ON (w.way_id = n.way_id) ");
    strcat (sql, "WHERE w.filtered = 1)");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE osm_nodes error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }
    return 1;
}

static int
filter_nodes (sqlite3 * handle, void *mask, int mask_len)
{
/* filtering any NODE to be exported */
    char sql[1024];
    int ret;
    sqlite3_stmt *query = NULL;
    sqlite3_stmt *stmt_nodes = NULL;
    sqlite3_stmt *stmt_ways = NULL;
    sqlite3_stmt *stmt_rels = NULL;
    char *sql_err = NULL;

/* the complete INSERT operation is handled as an unique SQL Transaction */
    ret = sqlite3_exec (handle, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }

/* preparing the UPDATE NODES statement */
    strcpy (sql, "UPDATE osm_nodes SET filtered = 1 WHERE node_id = ?");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt_nodes, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  return 0;
      }

/* preparing the UPDATE WAYS statement */
    strcpy (sql, "UPDATE osm_ways SET filtered = 1 WHERE way_id IN (");
    strcat (sql, "SELECT way_id FROM osm_way_refs WHERE node_id = ?)");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt_ways, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  return 0;
      }

/* preparing the UPDATE RELATIONS statement */
    strcpy (sql, "UPDATE osm_relations SET filtered = 1 WHERE rel_id IN (");
    strcat (sql,
	    "SELECT rel_id FROM osm_relation_refs WHERE type = 'N' AND ref = ?)");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt_rels, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  return 0;
      }

/* preparing the QUERY NODES statement */
    strcpy (sql, "SELECT node_id FROM osm_nodes ");
    strcat (sql, "WHERE MbrIntersects(Geometry, ?) = 1 ");
    strcat (sql, "AND ST_Intersects(Geometry, ?) = 1");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &query, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n%s\n", sql, sqlite3_errmsg (handle));
	  return 0;
      }
    sqlite3_bind_blob (query, 1, mask, mask_len, SQLITE_STATIC);
    sqlite3_bind_blob (query, 2, mask, mask_len, SQLITE_STATIC);

    while (1)
      {
	  /* scrolling the result set */
	  ret = sqlite3_step (query);
	  if (ret == SQLITE_DONE)
	    {
		/* there are no more rows to fetch - we can stop looping */
		break;
	    }
	  if (ret == SQLITE_ROW)
	    {
		/* ok, we've just fetched a valid row */
		sqlite3_int64 id = sqlite3_column_int64 (query, 0);

		/* marking this NODE as filtered */
		sqlite3_reset (stmt_nodes);
		sqlite3_clear_bindings (stmt_nodes);
		sqlite3_bind_int64 (stmt_nodes, 1, id);
		ret = sqlite3_step (stmt_nodes);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr, "sqlite3_step() error: UPDATE NODES\n");
		      goto stop;
		  }

		/* marking any dependent WAY as filtered */
		sqlite3_reset (stmt_ways);
		sqlite3_clear_bindings (stmt_ways);
		sqlite3_bind_int64 (stmt_ways, 1, id);
		ret = sqlite3_step (stmt_ways);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr, "sqlite3_step() error: UPDATE WAYS\n");
		      goto stop;
		  }

		/* marking any dependent RELATION as filtered */
		sqlite3_reset (stmt_rels);
		sqlite3_clear_bindings (stmt_rels);
		sqlite3_bind_int64 (stmt_rels, 1, id);
		ret = sqlite3_step (stmt_rels);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr,
			       "sqlite3_step() error: UPDATE RELATIONS\n");
		      goto stop;
		  }
	    }
	  else
	    {
		/* some unexpected error occurred */
		fprintf (stderr, "sqlite3_step() error: %s\n",
			 sqlite3_errmsg (handle));
		goto stop;
	    }
      }
    sqlite3_finalize (query);
    sqlite3_finalize (stmt_nodes);
    sqlite3_finalize (stmt_ways);
    sqlite3_finalize (stmt_rels);

/* committing the still pending SQL Transaction */
    ret = sqlite3_exec (handle, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return 0;
      }
    return 1;

  stop:
    if (query)
	sqlite3_finalize (query);
    if (stmt_nodes)
	sqlite3_finalize (stmt_nodes);
    if (stmt_ways)
	sqlite3_finalize (stmt_ways);
    if (stmt_rels)
	sqlite3_finalize (stmt_rels);
    return 0;
}

static int
parse_wkt_mask (const char *wkt_path, void **mask, int *mask_len)
{
/* acquiring the WKT mask */
    int cnt = 0;
    char *wkt_buf = NULL;
    char *p;
    int c;
    gaiaGeomCollPtr geom = NULL;

/* opening the text file containing the WKT mask */
    FILE *in = fopen (wkt_path, "r");
    if (in == NULL)
      {
	  fprintf (stderr, "Unable to open: %s\n", wkt_path);
	  return 0;
      }

/* counting how many chars are there */
    while (getc (in) != EOF)
	cnt++;
    if (cnt == 0)
      {
	  fprintf (stderr, "Empty file: %s\n", wkt_path);
	  fclose (in);
	  return 0;
      }

/* allocating the WKT buffer */
    wkt_buf = malloc (cnt + 1);
    memset (wkt_buf, '\0', cnt + 1);
    p = wkt_buf;
/* restarting the text file */
    rewind (in);

    while ((c = getc (in)) != EOF)
      {
	  /* loading the WKT buffer */
	  if ((p - wkt_buf) <= cnt)
	      *p++ = c;
      }
    *p = '\0';
    fclose (in);

/* attempting to parse the WKT expression */
    geom = gaiaParseWkt ((unsigned char *) wkt_buf, -1);
    free (wkt_buf);
    if (!geom)
	return 0;

/* converting to Binary Blob */
    gaiaToSpatiaLiteBlobWkb (geom, (unsigned char **) mask, mask_len);
    gaiaFreeGeomColl (geom);
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
    int node_id = 0;
    int version = 0;
    int timestamp = 0;
    int uid = 0;
    int user = 0;
    int changeset = 0;
    int filtered = 0;
    int geometry = 0;
    int sub = 0;
    int k = 0;
    int v = 0;
    int way_id = 0;
    int rel_id = 0;
    int role = 0;
    int ref = 0;
    const char *name;
    int i;
    char **results;
    int rows;
    int columns;

    *handle = NULL;
    printf ("SQLite version: %s\n", sqlite3_libversion ());
    printf ("SpatiaLite version: %s\n\n", spatialite_version ());

    ret = sqlite3_open_v2 (path, &db_handle, SQLITE_OPEN_READWRITE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", path,
		   sqlite3_errmsg (db_handle));
	  sqlite3_close (db_handle);
	  return;
      }
    spatialite_init_ex (db_handle, cache, 0);
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

/* checking the OSM_NODES table */
    strcpy (sql, "PRAGMA table_info(osm_nodes)");
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
		if (strcasecmp (name, "node_id") == 0)
		    node_id = 1;
		if (strcasecmp (name, "version") == 0)
		    version = 1;
		if (strcasecmp (name, "timestamp") == 0)
		    timestamp = 1;
		if (strcasecmp (name, "uid") == 0)
		    uid = 1;
		if (strcasecmp (name, "user") == 0)
		    user = 1;
		if (strcasecmp (name, "changeset") == 0)
		    changeset = 1;
		if (strcasecmp (name, "filtered") == 0)
		    filtered = 1;
		if (strcasecmp (name, "Geometry") == 0)
		    geometry = 1;
	    }
      }
    sqlite3_free_table (results);
    if (node_id && version && timestamp && uid && user && changeset && filtered
	&& geometry)
	;
    else
	goto unknown;

/* checking the OSM_NODE_TAGS table */
    strcpy (sql, "PRAGMA table_info(osm_node_tags)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  node_id = 0;
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "node_id") == 0)
		    node_id = 1;
		if (strcasecmp (name, "sub") == 0)
		    sub = 1;
		if (strcasecmp (name, "k") == 0)
		    k = 1;
		if (strcasecmp (name, "v") == 0)
		    v = 1;
	    }
      }
    sqlite3_free_table (results);
    if (node_id && sub && k && v)
	;
    else
	goto unknown;

/* checking the OSM_WAYS table */
    strcpy (sql, "PRAGMA table_info(osm_ways)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  version = 0;
	  timestamp = 0;
	  uid = 0;
	  user = 0;
	  changeset = 0;
	  filtered = 0;
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "way_id") == 0)
		    way_id = 1;
		if (strcasecmp (name, "version") == 0)
		    version = 1;
		if (strcasecmp (name, "timestamp") == 0)
		    timestamp = 1;
		if (strcasecmp (name, "uid") == 0)
		    uid = 1;
		if (strcasecmp (name, "user") == 0)
		    user = 1;
		if (strcasecmp (name, "changeset") == 0)
		    changeset = 1;
		if (strcasecmp (name, "filtered") == 0)
		    filtered = 1;
	    }
      }
    sqlite3_free_table (results);
    if (way_id && version && timestamp && uid && user && changeset && filtered)
	;
    else
	goto unknown;

/* checking the OSM_WAY_TAGS table */
    strcpy (sql, "PRAGMA table_info(osm_way_tags)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  way_id = 0;
	  sub = 0;
	  k = 0;
	  v = 0;
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "way_id") == 0)
		    way_id = 1;
		if (strcasecmp (name, "sub") == 0)
		    sub = 1;
		if (strcasecmp (name, "k") == 0)
		    k = 1;
		if (strcasecmp (name, "v") == 0)
		    v = 1;
	    }
      }
    sqlite3_free_table (results);
    if (way_id && sub && k && v)
	;
    else
	goto unknown;

/* checking the OSM_WAY_REFS table */
    strcpy (sql, "PRAGMA table_info(osm_way_refs)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  way_id = 0;
	  sub = 0;
	  node_id = 0;
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "way_id") == 0)
		    way_id = 1;
		if (strcasecmp (name, "sub") == 0)
		    sub = 1;
		if (strcasecmp (name, "node_id") == 0)
		    node_id = 1;
	    }
      }
    sqlite3_free_table (results);
    if (way_id && sub && node_id)
	;
    else
	goto unknown;

/* checking the OSM_RELATIONS table */
    strcpy (sql, "PRAGMA table_info(osm_relations)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  version = 0;
	  timestamp = 0;
	  uid = 0;
	  user = 0;
	  changeset = 0;
	  filtered = 0;
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "rel_id") == 0)
		    rel_id = 1;
		if (strcasecmp (name, "version") == 0)
		    version = 1;
		if (strcasecmp (name, "timestamp") == 0)
		    timestamp = 1;
		if (strcasecmp (name, "uid") == 0)
		    uid = 1;
		if (strcasecmp (name, "user") == 0)
		    user = 1;
		if (strcasecmp (name, "changeset") == 0)
		    changeset = 1;
		if (strcasecmp (name, "filtered") == 0)
		    filtered = 1;
	    }
      }
    sqlite3_free_table (results);
    if (rel_id && version && timestamp && uid && user && changeset && filtered)
	;
    else
	goto unknown;

/* checking the OSM_RELATION_TAGS table */
    strcpy (sql, "PRAGMA table_info(osm_relation_tags)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  rel_id = 0;
	  sub = 0;
	  k = 0;
	  v = 0;
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "rel_id") == 0)
		    rel_id = 1;
		if (strcasecmp (name, "sub") == 0)
		    sub = 1;
		if (strcasecmp (name, "k") == 0)
		    k = 1;
		if (strcasecmp (name, "v") == 0)
		    v = 1;
	    }
      }
    sqlite3_free_table (results);
    if (rel_id && sub && k && v)
	;
    else
	goto unknown;

/* checking the OSM_RELATION_REFS table */
    strcpy (sql, "PRAGMA table_info(osm_relation_refs)");
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
	goto unknown;
    if (rows < 1)
	;
    else
      {
	  rel_id = 0;
	  sub = 0;
	  type = 0;
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (strcasecmp (name, "rel_id") == 0)
		    rel_id = 1;
		if (strcasecmp (name, "sub") == 0)
		    sub = 1;
		if (strcasecmp (name, "type") == 0)
		    type = 1;
		if (strcasecmp (name, "ref") == 0)
		    ref = 1;
		if (strcasecmp (name, "role") == 0)
		    role = 1;
	    }
      }
    sqlite3_free_table (results);
    if (rel_id && sub && type && ref && role)
	;
    else
	goto unknown;

    *handle = db_handle;
    return;

  unknown:
    if (db_handle)
	sqlite3_close (db_handle);
    fprintf (stderr, "DB '%s'\n", path);
    fprintf (stderr, "doesn't seems to contain valid OSM-RAW data ...\n\n");
    return;
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_osm_filter ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr,
	     "-o or --osm-path pathname       the OSM-XML [output] file path\n");
    fprintf (stderr,
	     "-w or --wkt-mask-path pathname  path of text file [WKT mask]\n");
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
    const char *wkt_path = NULL;
    const char *db_path = NULL;
    int in_memory = 0;
    int cache_size = 0;
    int journal_off = 0;
    int error = 0;
    void *mask = NULL;
    int mask_len = 0;
    FILE *out = NULL;
    char *sql_err = NULL;
    int ret;
    void *cache;

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
		  case ARG_MASK_PATH:
		      wkt_path = argv[i];
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
	  if (strcmp (argv[i], "-w") == 0)
	    {
		next_arg = ARG_MASK_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--wkt-mask-path") == 0)
	    {
		next_arg = ARG_MASK_PATH;
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
    if (!wkt_path)
      {
	  fprintf (stderr,
		   "did you forget setting the --wkt-mask-path argument ?\n");
	  error = 1;
      }

    if (error)
      {
	  do_help ();
	  return -1;
      }

    if (!parse_wkt_mask (wkt_path, &mask, &mask_len))
      {
	  fprintf (stderr,
		   "ERROR: Invalid WKT mask [not a valid WKT expression]\n");
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

    out = fopen (osm_path, "wb");
    if (out == NULL)
	goto stop;

    if (journal_off)
      {
	  /* disabling the journal: unsafe but faster */
	  ret =
	      sqlite3_exec (handle, "PRAGMA journal_mode = OFF", NULL, NULL,
			    &sql_err);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "PRAGMA journal_mode=OFF error: %s\n",
			 sql_err);
		sqlite3_free (sql_err);
		goto stop;
	    }
      }

/* resetting filtered nodes, ways and relations */
    if (!reset_filtered (handle))
	goto stop;

/* identifying filtered nodes */
    if (!filter_nodes (handle, mask, mask_len))
	goto stop;

/* identifying relations depending on other relations */
    if (!filter_rel_relations (handle))
	goto stop;

/* identifying ways depending on relations */
    if (!filter_way_relations (handle))
	goto stop;

/* identifying nodes depending on relations */
    if (!filter_node_relations (handle))
	goto stop;

/* identifying nodes depending on ways */
    if (!filter_node_ways (handle))
	goto stop;

/* writing the OSM header */
    fprintf (out, "<?xml version='1.0' encoding='UTF-8'?>\n");
    fprintf (out, "<osm version=\"0.6\" generator=\"splite2osm\">\n");

    fprintf (stderr, "OutNodes\n");
/* exporting OSM NODES */
    if (!do_output_nodes (out, handle))
      {
	  fprintf (stderr, "\nThe output OSM file is corrupted !!!\n");
	  goto stop;
      }

    fprintf (stderr, "OutWays\n");
/* exporting OSM WAYS */
    if (!do_output_ways (out, handle))
      {
	  fprintf (stderr, "\nThe output OSM file is corrupted !!!\n");
	  goto stop;
      }

    fprintf (stderr, "OutRelations\n");
/* exporting OSM RELATIONS */
    if (!do_output_relations (out, handle))
      {
	  fprintf (stderr, "\nThe output OSM file is corrupted !!!\n");
	  goto stop;
      }

/* writing the OSM footer */
    fprintf (out, "</osm>\n");

  stop:
    free (mask);
    sqlite3_close (handle);
    spatialite_cleanup_ex (cache);
    if (out != NULL)
	fclose (out);
    spatialite_shutdown ();
    return 0;
}
