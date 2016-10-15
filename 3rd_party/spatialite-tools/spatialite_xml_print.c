/* 
/ spatialite_xml_print
/
/ a tool printing an XML file from SQLite tables
/
/ version 1.0, 2014 July 1
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

/*
 
CREDITS:

inital development of the XML tools has been funded by:
Regione Toscana - Settore Sistema Informativo Territoriale ed Ambientale

*/

#include <sys/time.h>

#if defined(_WIN32) && !defined(__MINGW32__)
/* MSVC strictly requires this include [off_t] */
#include <sys/types.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#ifdef SPATIALITE_AMALGAMATION
#include <spatialite/sqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <spatialite.h>
#include <spatialite/gaiaaux.h>

#define ARG_NONE	0
#define ARG_XML_PATH	1
#define ARG_DB_PATH	2
#define ARG_CACHE_SIZE 3

struct xml_attr
{
/* a struct wrapping an XML attribute */
    char *attr_prefix;
    char *attr_name;
    struct xml_attr *next;
};

struct xml_node
{
/* a struct wrapping an XML node */
    int level;
    char *xml_prefix;
    char *xml_tag;
    char *table;
    char *parent_table;
    sqlite3_stmt *stmt;
    struct xml_attr *first_attr;
    struct xml_attr *last_attr;
    struct xml_node *first;
    struct xml_node *last;
    struct xml_node *next;
};

static void
do_destroy_node (struct xml_node *node)
{
/* memory cleanup - destroying an XML Node */
    struct xml_attr *pa;
    struct xml_attr *pan;
    struct xml_node *pn;
    struct xml_node *pnn;
    if (node == NULL)
	return;
    if (node->xml_prefix != NULL)
	free (node->xml_prefix);
    if (node->xml_tag != NULL)
	free (node->xml_tag);
    if (node->table != NULL)
	free (node->table);
    if (node->parent_table != NULL)
	free (node->parent_table);
    if (node->stmt != NULL)
	sqlite3_finalize (node->stmt);
    pa = node->first_attr;
    while (pa != NULL)
      {
	  pan = pa->next;
	  if (pa->attr_prefix != NULL)
	      free (pa->attr_prefix);
	  if (pa->attr_name != NULL)
	      free (pa->attr_name);
	  free (pa);
	  pa = pan;
      }
    pn = node->first;
    while (pn != NULL)
      {
	  pnn = pn->next;
	  do_destroy_node (pn);
	  pn = pnn;
      }
    free (node);
}

static struct xml_node *
alloc_xml_node (sqlite3 * sqlite, int level, const char *namespace,
		const char *tag, const char *table, const char *parent)
{
/* creating an XML Node */
    int len;
    char *sql;
    char *xtable;
    int ret;
    sqlite3_stmt *stmt;
    struct xml_node *node = malloc (sizeof (struct xml_node));
    node->level = level;
    if (namespace == NULL)
	node->xml_prefix = NULL;
    else
      {
	  len = strlen (namespace);
	  node->xml_prefix = malloc (len + 1);
	  strcpy (node->xml_prefix, namespace);
      }
    len = strlen (tag);
    node->xml_tag = malloc (len + 1);
    strcpy (node->xml_tag, tag);
    len = strlen (table);
    node->table = malloc (len + 1);
    strcpy (node->table, table);
    if (parent == NULL)
	node->parent_table = NULL;
    else
      {
	  len = strlen (parent);
	  node->parent_table = malloc (len + 1);
	  strcpy (node->parent_table, parent);
      }
    xtable = gaiaDoubleQuotedSql (table);
    if (parent == NULL)
	sql = sqlite3_mprintf ("SELECT * FROM \"%s\"", xtable);
    else
	sql =
	    sqlite3_mprintf ("SELECT * FROM \"%s\" WHERE parent_id = ?",
			     xtable);
    free (xtable);
    node->stmt = NULL;
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT FROM xml_node error: %s\n",
		   sqlite3_errmsg (sqlite));
	  return NULL;
      }
    node->stmt = stmt;
    node->first = NULL;
    node->last = NULL;
    node->first_attr = NULL;
    node->last_attr = NULL;
    node->next = NULL;
    return node;
}

static struct xml_node *
find_node_by_table (struct xml_node *node, const char *table)
{
/* recursively searching the XML Tree */
    struct xml_node *pn;
    struct xml_node *res;
    if (strcasecmp (node->table, table) == 0)
	return node;
    pn = node->first;
    while (pn != NULL)
      {
	  /* searching within children */
	  res = find_node_by_table (pn, table);
	  if (res != NULL)
	      return res;
	  pn = pn->next;
      }
    return NULL;
}

static int
do_add_xml_node (sqlite3 * sqlite, struct xml_node *master, int level,
		 const char *namespace, const char *tag, const char *table,
		 const char *parent)
{
/* inserting a new node into the XML Tree */
    struct xml_node *node;
    struct xml_node *parent_node;
    if (master == NULL)
	return 0;
    node = alloc_xml_node (sqlite, level, namespace, tag, table, parent);
    parent_node = find_node_by_table (master, parent);
    if (parent_node == NULL)
	return 0;
/* appending the child into the parent */
    if (parent_node->first == NULL)
	parent_node->first = node;
    if (parent_node->last != NULL)
	parent_node->last->next = node;
    parent_node->last = node;
    return 1;
}

static char *
extract_attribute_prefix (struct xml_node *node, const char *reference)
{
    char *start;
    int i;
    if (node->xml_prefix == NULL)
	start = sqlite3_mprintf ("<%s", node->xml_tag);
    else
	start = sqlite3_mprintf ("<%s:%s ", node->xml_prefix, node->xml_tag);

    if (strncasecmp (start, reference, strlen (start)) == 0)
      {
	  int a = strlen (start);
	  int b = -1;
	  for (i = a; i < (int) strlen (reference); i++)
	    {
		if (*(reference + i) == ':')
		  {
		      b = i;
		      break;
		  }
	    }
	  sqlite3_free (start);
	  if (b > a)
	    {
		int len = b - a;
		char *prefix = malloc (len + 1);
		memcpy (prefix, reference + a, len);
		*(prefix + len) = '\0';
		return prefix;
	    }
	  return NULL;
      }
    else
	fprintf (stderr, "ERROR: invalid Attribute NameSpace\n");
    sqlite3_free (start);
    return NULL;
}

static void
do_add_attribute (struct xml_node *node, char *prefix, const char *attribute)
{
/* creating and inserting a new XML attribute */
    int len;
    struct xml_attr *attr = malloc (sizeof (struct xml_attr));
    attr->attr_prefix = prefix;
    len = strlen (attribute);
    attr->attr_name = malloc (len + 1);
    strcpy (attr->attr_name, attribute);
    attr->next = NULL;
/* inserting into the XML Node */
    if (node->first_attr == NULL)
	node->first_attr = attr;
    if (node->last_attr != NULL)
	node->last_attr->next = attr;
    node->last_attr = attr;
}

struct xml_node *
do_build_master (sqlite3 * sqlite)
{
/* attempting to build the XML tree */
    int ret;
    const char *sql;
    sqlite3_stmt *stmt = NULL;
    struct xml_node *master = NULL;
    struct xml_node *current = NULL;
    int first = 1;

/* extracting XML Nodes from MetaCatalog */
    sql =
	"SELECT tree_level, xml_tag_namespace, xml_tag_name, table_name, parent_table_name "
	"FROM xml_metacatalog_tables " "ORDER BY tree_level, rowid";
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT FROM xml_metacatalog_tables error: %s\n",
		   sqlite3_errmsg (sqlite));
	  return NULL;
      }
    while (1)
      {
	  /* fetching the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* fetching a row */
		const char *namespace = NULL;
		const char *tag = NULL;
		const char *table = NULL;
		const char *parent = NULL;
		int level = sqlite3_column_int (stmt, 0);
		if (sqlite3_column_type (stmt, 1) == SQLITE_NULL)
		    ;
		else
		    namespace = (const char *) sqlite3_column_text (stmt, 1);
		if (sqlite3_column_type (stmt, 2) == SQLITE_NULL)
		    ;
		else
		    tag = (const char *) sqlite3_column_text (stmt, 2);
		if (sqlite3_column_type (stmt, 3) == SQLITE_NULL)
		    ;
		else
		    table = (const char *) sqlite3_column_text (stmt, 3);
		if (sqlite3_column_type (stmt, 4) == SQLITE_NULL)
		    ;
		else
		    parent = (const char *) sqlite3_column_text (stmt, 4);
		if (first)
		  {
		      /* this is the ROOT node */
		      if (parent == NULL)
			  master =
			      alloc_xml_node (sqlite, level, namespace, tag,
					      table, NULL);
		      first = 0;
		      if (master == NULL)
			{
			    fprintf (stderr,
				     "ERROR: unable to build the XML Tree\n");
			    goto error;
			}
		  }
		else
		  {
		      if (!do_add_xml_node
			  (sqlite, master, level, namespace, tag, table,
			   parent))
			{
			    fprintf (stderr,
				     "ERROR: unable to build the XML Tree\n");
			    goto error;
			}
		  }
	    }
	  else
	    {
		fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
		goto error;
	    }
      }
    sqlite3_finalize (stmt);

/* extract XML Attributes from MetaCatalog */
    sql = "SELECT table_name, column_name, xml_reference "
	"FROM xml_metacatalog_columns "
	"WHERE Upper(column_name) NOT IN (Upper('node_id'), Upper('parent_id'), "
	"Upper('node_value'), Upper('from_gml_geometry')) "
	"ORDER BY table_name, ROWID";
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT FROM xml_metacatalog_columns error: %s\n",
		   sqlite3_errmsg (sqlite));
	  return NULL;
      }
    while (1)
      {
	  /* fetching the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* fetching a row */
		const char *table =
		    (const char *) sqlite3_column_text (stmt, 0);
		const char *attribute =
		    (const char *) sqlite3_column_text (stmt, 1);
		const char *reference =
		    (const char *) sqlite3_column_text (stmt, 2);
		char *prefix = NULL;
		if (current == NULL)
		    current = find_node_by_table (master, table);
		if (strcasecmp (current->table, table) != 0)
		    current = find_node_by_table (master, table);
		if (current == NULL || strcasecmp (current->table, table) != 0)
		  {
		      fprintf (stderr, "ERROR: unable to build the XML Tree\n");
		      goto error;
		  }
		prefix = extract_attribute_prefix (current, reference);
		do_add_attribute (current, prefix, attribute);
	    }
	  else
	    {
		fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
		goto error;
	    }
      }
    sqlite3_finalize (stmt);

    return master;

  error:
    if (stmt != NULL)
	sqlite3_finalize (stmt);
    if (master != NULL)
	do_destroy_node (master);
    return NULL;
}

static char *
clean_xml (const char *dirty)
{
/* well formatting an XML value */
    char *clean;
    char *out;
    int i;
    int extra = 0;
    int len = strlen (dirty);
    for (i = 0; i < len; i++)
      {
	  /* computing the required extra-length */
	  switch (*(dirty + i))
	    {
	    case '&':
		extra += 4;
		break;
	    case '<':
		extra += 3;
		break;
	    case '>':
		extra += 3;
		break;
	    case '\'':
		extra += 5;
		break;
	    case '"':
		extra += 5;
		break;
	    };
      }
    clean = malloc (len + extra + 1);
    out = clean;
    for (i = 0; i < len; i++)
      {
	  /* computing the required extra-length */
	  switch (*(dirty + i))
	    {
	    case '&':
		*out++ = '&';
		*out++ = 'a';
		*out++ = 'm';
		*out++ = 'p';
		*out++ = ';';
		break;
	    case '<':
		*out++ = '&';
		*out++ = 'l';
		*out++ = 't';
		*out++ = ';';
		break;
	    case '>':
		*out++ = '&';
		*out++ = 'g';
		*out++ = 't';
		*out++ = ';';
		break;
	    case '\'':
		*out++ = '&';
		*out++ = 'a';
		*out++ = 'p';
		*out++ = 'o';
		*out++ = 's';
		*out++ = ';';
		break;
	    case '"':
		*out++ = '&';
		*out++ = 'q';
		*out++ = 'u';
		*out++ = 'o';
		*out++ = 't';
		*out++ = ';';
		break;
	    default:
		*out++ = *(dirty + i);
		break;
	    };
      }
    *out = '\0';
    return clean;
}

static void
do_print_xml_child (FILE * out, struct xml_node *node, sqlite3_int64 parent_id,
		    sqlite3 * sqlite)
{
/* recursively printing the XML document - Child Node */
    int ret;
    char *indent;
    int i;
    sqlite3_stmt *stmt = node->stmt;
    if (stmt == NULL)
	return;

    indent = malloc (node->level + 1);
    for (i = 0; i < node->level; i++)
	*(indent + i) = '\t';
    *(indent + node->level) = '\0';

    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_int64 (stmt, 1, parent_id);
    while (1)
      {
	  /* fetching the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* fetching a row */
		struct xml_attr *attr;
		struct xml_node *child;
		char *xprefix;
		char *xtag;
		char *xvalue;
		sqlite3_int64 node_id;
		/* printing the XML Node start-tag */
		if (node->xml_prefix == NULL)
		  {
		      xtag = clean_xml (node->xml_tag);
		      fprintf (out, "%s<%s", indent, xtag);
		      free (xtag);
		  }
		else
		  {
		      xprefix = clean_xml (node->xml_prefix);
		      xtag = clean_xml (node->xml_tag);
		      fprintf (out, "%s<%s:%s", indent, xprefix, xtag);
		      free (xprefix);
		      free (xtag);
		  }
		attr = node->first_attr;
		while (attr != NULL)
		  {
		      /* printing all Node Attributes */
		      const char *value = NULL;
		      for (i = 0; i < sqlite3_column_count (stmt); i++)
			{
			    const char *column = sqlite3_column_name (stmt, i);
			    if (strcasecmp (column, attr->attr_name) == 0)
			      {
				  if (sqlite3_column_type (stmt, i) ==
				      SQLITE_NULL)
				      ;
				  else
				      value =
					  (const char *)
					  sqlite3_column_text (stmt, i);
				  break;
			      }
			}
		      if (value == NULL)
			{
			    /* skipping NULL attributes */
			    attr = attr->next;
			    continue;
			}
		      if (attr->attr_prefix == NULL)
			{
			    xtag = clean_xml (attr->attr_name);
			    xvalue = clean_xml (value);
			    fprintf (out, " %s=\"%s\"", xtag, xvalue);
			    free (xtag);
			    free (xvalue);
			}
		      else
			{
			    xprefix = clean_xml (attr->attr_prefix);
			    xtag = clean_xml (attr->attr_name);
			    xvalue = clean_xml (value);
			    fprintf (out, " %s:%s=\"%s\"", xprefix, xtag,
				     xvalue);
			    free (xprefix);
			    free (xtag);
			    free (xvalue);
			}
		      attr = attr->next;
		  }
		if (node->first == NULL)
		  {
		      /* we have no Childern nodes for sure */
		      int from_gml_geometry = 0;
		      const char *value = NULL;
		      xvalue = NULL;
		      for (i = 0; i < sqlite3_column_count (stmt); i++)
			{
			    const char *column = sqlite3_column_name (stmt, i);
			    if (strcasecmp (column, "from_gml_geometry") == 0)
				from_gml_geometry = 1;
			    if (strcasecmp (column, "node_value") == 0)
			      {
				  if (sqlite3_column_type (stmt, i) !=
				      SQLITE_NULL)
				      value =
					  (const char *)
					  sqlite3_column_text (stmt, i);
			      }
			}
		      if (value == NULL)
			  fprintf (out, " />\n");
		      else
			{
			    if (!from_gml_geometry)
				xvalue = clean_xml (value);
			    else
				xvalue = (char *) value;
			    if (node->xml_prefix == NULL)
			      {
				  xtag = clean_xml (node->xml_tag);
				  fprintf (out, ">%s</%s>\n", xvalue, xtag);
				  free (xtag);
			      }
			    else
			      {
				  xprefix = clean_xml (node->xml_prefix);
				  xtag = clean_xml (node->xml_tag);
				  fprintf (out, ">%s</%s:%s>\n", xvalue,
					   xprefix, xtag);
				  free (xprefix);
				  free (xtag);
			      }
			    if (!from_gml_geometry)
				free (xvalue);
			}
		  }
		else
		  {
		      int i;
		      fprintf (out, ">\n");
		      node_id = 0;
		      for (i = 0; i < sqlite3_column_count (stmt); i++)
			{
			    const char *column = sqlite3_column_name (stmt, i);
			    if (strcasecmp (column, "node_id") == 0)
			      {
				  node_id = sqlite3_column_int64 (stmt, i);
				  break;
			      }
			}
		      child = node->first;
		      while (child != NULL)
			{
			    /* recursively printing all Children nodes */
			    do_print_xml_child (out, child, node_id, sqlite);
			    child = child->next;
			}
		      /* printing the XML Node end-tag */
		      if (node->xml_prefix == NULL)
			{
			    xtag = clean_xml (node->xml_tag);
			    fprintf (out, "%s</%s>\n", indent, xtag);
			    free (xtag);
			}
		      else
			{
			    xprefix = clean_xml (node->xml_prefix);
			    xtag = clean_xml (node->xml_tag);
			    fprintf (out, "%s</%s:%s>\n", indent, xprefix,
				     xtag);
			    free (xprefix);
			    free (xtag);
			}
		  }
	    }
	  else
	    {
		fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
		free (indent);
		return;
	    }
      }
    free (indent);
}

static void
do_print_xml (FILE * out, struct xml_node *node, sqlite3 * sqlite)
{
/* printing the XML document - ROOT Node */
    int ret;
    int i;
    sqlite3_stmt *stmt = node->stmt;
    if (stmt == NULL)
	return;

    fprintf (out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    while (1)
      {
	  /* fetching the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* fetching a row */
		struct xml_attr *attr;
		struct xml_node *child;
		char *xprefix;
		char *xtag;
		char *xvalue;
		sqlite3_int64 node_id;
		/* printing the XML Node start-tag */
		if (node->xml_prefix == NULL)
		  {
		      xtag = clean_xml (node->xml_tag);
		      fprintf (out, "<%s", xtag);
		      free (xtag);
		  }
		else
		  {
		      xprefix = clean_xml (node->xml_prefix);
		      xtag = clean_xml (node->xml_tag);
		      fprintf (out, "<%s:%s", xprefix, xtag);
		      free (xprefix);
		      free (xtag);
		  }
		attr = node->first_attr;
		while (attr != NULL)
		  {
		      /* printing all Node Attributes */
		      const char *value = NULL;
		      for (i = 0; i < sqlite3_column_count (stmt); i++)
			{
			    const char *column = sqlite3_column_name (stmt, i);
			    if (strcasecmp (column, attr->attr_name) == 0)
			      {
				  if (sqlite3_column_type (stmt, i) ==
				      SQLITE_NULL)
				      ;
				  else
				      value =
					  (const char *)
					  sqlite3_column_text (stmt, i);
				  break;
			      }
			}
		      if (value == NULL)
			{
			    /* skipping NULL attributes */
			    attr = attr->next;
			    continue;
			}
		      if (attr->attr_prefix == NULL)
			{
			    xtag = clean_xml (attr->attr_name);
			    xvalue = clean_xml (value);
			    fprintf (out, " %s=\"%s\"", xtag, xvalue);
			    free (xtag);
			    free (xvalue);
			}
		      else
			{
			    xprefix = clean_xml (attr->attr_prefix);
			    xtag = clean_xml (attr->attr_name);
			    xvalue = clean_xml (value);
			    fprintf (out, " %s:%s=\"%s\"", xprefix, xtag,
				     xvalue);
			    free (xprefix);
			    free (xtag);
			    free (xvalue);
			}
		      attr = attr->next;
		  }
		if (node->first == NULL)
		  {
		      /* we have no Childern nodes for sure */
		      int from_gml_geometry = 0;
		      const char *value = NULL;
		      xvalue = NULL;
		      for (i = 0; i < sqlite3_column_count (stmt); i++)
			{
			    const char *column = sqlite3_column_name (stmt, i);
			    if (strcasecmp (column, "from_gml_geometry") == 0)
				from_gml_geometry = 1;
			    if (strcasecmp (column, "node_value") == 0)
			      {
				  if (sqlite3_column_type (stmt, i) !=
				      SQLITE_NULL)
				      value =
					  (const char *)
					  sqlite3_column_text (stmt, i);
			      }
			}
		      if (value == NULL)
			  fprintf (out, " />\n");
		      else
			{
			    if (!from_gml_geometry)
				xvalue = clean_xml (value);
			    else
				xvalue = (char *) value;
			    if (node->xml_prefix == NULL)
			      {
				  xtag = clean_xml (node->xml_tag);
				  fprintf (out, ">%s</%s>\n", xvalue, xtag);
				  free (xtag);
			      }
			    else
			      {
				  xprefix = clean_xml (node->xml_prefix);
				  xtag = clean_xml (node->xml_tag);
				  fprintf (out, ">%s</%s:%s>\n", xvalue,
					   xprefix, xtag);
				  free (xprefix);
				  free (xtag);
			      }
			    if (!from_gml_geometry)
				free (xvalue);
			}
		  }
		else
		  {
		      fprintf (out, ">\n");
		      node_id = 0;
		      for (i = 0; i < sqlite3_column_count (stmt); i++)
			{
			    const char *column = sqlite3_column_name (stmt, i);
			    if (strcasecmp (column, "node_id") == 0)
			      {
				  node_id = sqlite3_column_int64 (stmt, i);
				  break;
			      }
			}
		      child = node->first;
		      while (child != NULL)
			{
			    /* recursively printing all Children nodes */
			    do_print_xml_child (out, child, node_id, sqlite);
			    child = child->next;
			}
		      /* printing the XML Node end-tag */
		      if (node->xml_prefix == NULL)
			{
			    xtag = clean_xml (node->xml_tag);
			    fprintf (out, "</%s>\n", xtag);
			    free (xtag);
			}
		      else
			{
			    xprefix = clean_xml (node->xml_prefix);
			    xtag = clean_xml (node->xml_tag);
			    fprintf (out, "</%s:%s>\n", xprefix, xtag);
			    free (xprefix);
			    free (xtag);
			}
		  }
	    }
	  else
	    {
		fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
		return;
	    }
      }
}

static void
open_db (const char *path, sqlite3 ** handle, int cache_size, void *cache)
{
/* opening the DB */
    sqlite3 *db_handle;
    int ret;
    char sql[1024];

    *handle = NULL;
    printf ("SQLite version: %s\n", sqlite3_libversion ());
    printf ("SpatiaLite version: %s\n", spatialite_version ());

    ret = sqlite3_open_v2 (path, &db_handle, SQLITE_OPEN_READONLY, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", path,
		   sqlite3_errmsg (db_handle));
	  sqlite3_close (db_handle);
	  db_handle = NULL;
	  return;
      }
    spatialite_init_ex (db_handle, cache, 0);

    if (cache_size > 0)
      {
	  /* setting the CACHE-SIZE */
	  sprintf (sql, "PRAGMA cache_size=%d", cache_size);
	  sqlite3_exec (db_handle, sql, NULL, NULL, NULL);
      }

/* enabling PK/FK constraints */
    sqlite3_exec (db_handle, "PRAGMA foreign_keys = 1", NULL, NULL, NULL);
    *handle = db_handle;
    return;
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_xml_printf ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr,
	     "-d or --db-path     pathname    the SpatiaLite DB [INPUT] path\n\n");
    fprintf (stderr,
	     "-x or --xml-path    pathname    the XML file [OUTPUT] path\n");
    fprintf (stderr,
	     "-cs or --cache-size    num      DB cache size (how many pages)\n");
    fprintf (stderr,
	     "-m or --in-memory               using IN-MEMORY database\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function mainly perform arguments checking */
    sqlite3 *handle;
    int i;
    int next_arg = ARG_NONE;
    const char *xml_path = NULL;
    const char *db_path = NULL;
    int cache_size = 0;
    int error = 0;
    void *cache;
    struct xml_node *master = NULL;
    FILE *out = NULL;

    for (i = 1; i < argc; i++)
      {
	  /* parsing the invocation arguments */
	  if (next_arg != ARG_NONE)
	    {
		switch (next_arg)
		  {
		  case ARG_XML_PATH:
		      xml_path = argv[i];
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
	  if (strcmp (argv[i], "-x") == 0)
	    {
		next_arg = ARG_XML_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--xml-path") == 0)
	    {
		next_arg = ARG_XML_PATH;
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
	  fprintf (stderr, "unknown argument: %s\n", argv[i]);
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }

/* checking the arguments */
    if (!xml_path)
      {
	  fprintf (stderr,
		   "did you forget setting the --xml-path argument ?\n");
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
    cache = spatialite_alloc_connection ();
    open_db (db_path, &handle, cache_size, cache);
    if (!handle)
	return -1;

    printf ("Input DB: %s\n", db_path);

/* preparing the XML Tree */
    master = do_build_master (handle);
    if (master == NULL)
	goto error;

/* opening the XML output file */
    out = fopen (xml_path, "w");
    if (out == NULL)
      {
	  fprintf (stderr,
		   "Unable to create/open the output destination \"%s\"\n",
		   xml_path);
      }

/* printing the output XML */
    do_print_xml (out, master, handle);

  error:
    if (out != NULL)
	fclose (out);
    if (master != NULL)
	do_destroy_node (master);
    sqlite3_close (handle);
    spatialite_cleanup_ex (cache);
    return 0;
}
