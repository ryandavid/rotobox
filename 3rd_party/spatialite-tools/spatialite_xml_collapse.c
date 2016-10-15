/* 
/ spatialite_xml_collapse
/
/ a tool performing post-processing tasks after importing
/ several XML files into SQLite tables
/
/ version 1.0, 2013 August 20
/
/ Author: Sandro Furieri a.furieri@lqt.it
/
/ Copyright (C) 2013  Alessandro Furieri
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

#if defined(_WIN32) && !defined(__MINGW32__)
/* MSVC strictly requires this include [off_t] */
#include <sys/types.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sqlite3.h>
#include <spatialite.h>

#define ARG_NONE	0
#define ARG_DB_PATH	1
#define ARG_CACHE_SIZE 2
#define ARG_NAME_LEVEL 3

struct resultset_values
{
/* a struct wrapping values from a resultset */
    int type;
    sqlite3_int64 int_value;
    double dbl_value;
    unsigned char *txt_blob_value;
    int txt_blob_size;
};

struct resultset_comparator
{
/* object for comparing two rows of the same resultset */
    struct resultset_values *previous;
    struct resultset_values *current;
    int num_columns;
    sqlite3_int64 previous_rowid;
    sqlite3_int64 current_rowid;
};

struct xml_attribute
{
/* a struct wrapping an XML attribute (column name) */
    char *attr_name;
    char *sql_name;
    int main_collapsed;
    int already_defined;
    int datatype;
    sqlite3_int64 int_value;
    double double_value;
    const unsigned char *text_value;
    const unsigned char *blob_value;
    int blob_size;
    char *xml_reference;
    struct xml_attribute *next;
};

struct new_attributes
{
/* containing struct for XML attributes (columns) to be added */
    struct xml_attribute *first;
    struct xml_attribute *last;
    int collision;
};

struct child_reference
{
/* an XML child reference */
    struct xml_table *child;
    struct child_reference *next;
};

struct xml_table
{
/* a struct wrapping an XML table */
    char *table_name;
    char *parent_table;
    char *tag_ns;
    char *tag_name;
    char *geometry;
    struct xml_table *parent;
    struct child_reference *first;
    struct child_reference *last;
    int done;
    int level;
    struct xml_table *next;
};

struct xml_geometry
{
/* a struct wrapping some BLOB geometry */
    char *table_name;
    char *geometry;
    char *type;
    int srid;
    char *dims;
    struct xml_geometry *next;
};

struct xml_tables_list
{
/* linked list of XML tables */
    struct xml_table *first;
    struct xml_table *last;
    struct xml_geometry *first_geom;
    struct xml_geometry *last_geom;
};

static struct resultset_comparator *
create_resultset_comparator (int columns)
{
/* creating an empty resultset comparator object */
    int i;
    struct resultset_comparator *p =
	malloc (sizeof (struct resultset_comparator));
    p->num_columns = columns;
    p->previous_rowid = -1;
    p->current_rowid = -1;
    p->previous = malloc (sizeof (struct resultset_values) * columns);
    p->current = malloc (sizeof (struct resultset_values) * columns);
    for (i = 0; i < columns; i++)
      {
	  struct resultset_values *value = p->previous + i;
	  value->type = SQLITE_NULL;
	  value->txt_blob_value = NULL;
	  value = p->current + i;
	  value->type = SQLITE_NULL;
	  value->txt_blob_value = NULL;
      }
    return p;
}

static void
destroy_resultset_comparator (struct resultset_comparator *ptr)
{
/* memory cleanup - destroying a resultset comparator object */
    int i;
    if (ptr == NULL)
	return;
    for (i = 0; i < ptr->num_columns; i++)
      {
	  struct resultset_values *value = ptr->previous + i;
	  if (value->txt_blob_value != NULL)
	      free (value->txt_blob_value);
	  value = ptr->current + i;
	  if (value->txt_blob_value != NULL)
	      free (value->txt_blob_value);
      }
    if (ptr->previous != NULL)
	free (ptr->previous);
    if (ptr->current != NULL)
	free (ptr->current);
    free (ptr);
}

static void
save_row_from_resultset (struct resultset_comparator *ptr, sqlite3_stmt * stmt)
{
/* saving the current row values */
    int i;
    int size;
    const unsigned char *p;
    if (ptr == NULL)
	return;
    ptr->current_rowid = sqlite3_column_int64 (stmt, 0);
    for (i = 0; i < ptr->num_columns; i++)
      {
	  struct resultset_values *value = ptr->current + i;
	  value->type = sqlite3_column_type (stmt, i + 1);
	  switch (value->type)
	    {
	    case SQLITE_INTEGER:
		value->int_value = sqlite3_column_int64 (stmt, i + 1);
		break;
	    case SQLITE_FLOAT:
		value->dbl_value = sqlite3_column_double (stmt, i + 1);
		break;
	    case SQLITE_TEXT:
		p = sqlite3_column_text (stmt, i + 1);
		size = strlen ((const char *) p);
		value->txt_blob_value = malloc (size + 1);
		strcpy ((char *) (value->txt_blob_value), (const char *) p);
		break;
	    case SQLITE_BLOB:
		p = sqlite3_column_blob (stmt, i + 1);
		size = sqlite3_column_bytes (stmt, i + 1);
		value->txt_blob_value = malloc (size);
		memcpy (value->txt_blob_value, p, size);
		value->txt_blob_size = size;
		break;
	    };
      }
}

static int
resultset_rows_equals (struct resultset_comparator *ptr)
{
/* comparing the current and previous row from the resultset */
    int i;
    if (ptr == NULL)
	return 0;
    for (i = 0; i < ptr->num_columns; i++)
      {
	  struct resultset_values *val_prev = ptr->previous + i;
	  struct resultset_values *val_curr = ptr->current + i;
	  if (val_prev->type != val_curr->type)
	      return 0;
	  switch (val_prev->type)
	    {
	    case SQLITE_INTEGER:
		if (val_prev->int_value != val_curr->int_value)
		    return 0;
		break;
	    case SQLITE_FLOAT:
		if (val_prev->dbl_value != val_curr->dbl_value)
		    return 0;
		break;
	    case SQLITE_TEXT:
		if (strcmp
		    ((const char *) (val_prev->txt_blob_value),
		     (const char *) (val_curr->txt_blob_value)) != 0)
		    return 0;
		break;
	    case SQLITE_BLOB:
		if (val_prev->txt_blob_size != val_curr->txt_blob_size)
		    return 0;
		if (memcmp
		    (val_prev->txt_blob_value, val_curr->txt_blob_value,
		     val_curr->txt_blob_size) != 0)
		    return 0;
		break;
	    };
      }
    return 1;
}

static sqlite3_int64
get_current_resultset_rowid (struct resultset_comparator *ptr)
{
/* returns the current ROWID */
    if (ptr == NULL)
	return -1;
    return ptr->current_rowid;
}

static void
reset_resultset_current_row (struct resultset_comparator *ptr)
{
/* resetting the resultset current row values */
    int i;
    if (ptr == NULL)
	return;
    ptr->current_rowid = -1;
    for (i = 0; i < ptr->num_columns; i++)
      {
	  struct resultset_values *value = ptr->current + i;
	  value->type = SQLITE_NULL;
	  if (value->txt_blob_value != NULL)
	      free (value->txt_blob_value);
	  value->txt_blob_value = NULL;
      }
}

static void
swap_resultset_rows (struct resultset_comparator *ptr)
{
/* resetting the resultset comparator */
    int i;
    if (ptr == NULL)
	return;
    ptr->previous_rowid = ptr->current_rowid;
    ptr->current_rowid = -1;
    for (i = 0; i < ptr->num_columns; i++)
      {
	  struct resultset_values *val_prev = ptr->previous + i;
	  struct resultset_values *val_curr = ptr->current + i;
	  if (val_prev->txt_blob_value != NULL)
	      free (val_prev->txt_blob_value);
	  val_prev->type = val_curr->type;
	  val_prev->int_value = val_curr->int_value;
	  val_prev->dbl_value = val_curr->dbl_value;
	  val_prev->txt_blob_value = val_curr->txt_blob_value;
	  val_prev->txt_blob_size = val_curr->txt_blob_size;
	  val_curr->type = SQLITE_NULL;
	  val_curr->txt_blob_value = NULL;
      }
}

static void
destroy_child_reference (struct child_reference *p)
{
/* memory cleanup - destroying a child reference */
    if (p == NULL)
	return;
    free (p);
}

static struct xml_table *
alloc_xml_table (const char *table, const char *parent, const char *tag_ns,
		 const char *tag_name, const char *geometry, int level)
{
/* allocating and initializing an XML table */
    int len;
    struct xml_table *p = malloc (sizeof (struct xml_table));
    len = strlen (table);
    p->table_name = malloc (len + 1);
    strcpy (p->table_name, table);
    if (parent == NULL)
	p->parent_table = NULL;
    else
      {
	  len = strlen (parent);
	  p->parent_table = malloc (len + 1);
	  strcpy (p->parent_table, parent);
      }
    if (tag_ns == NULL)
	p->tag_ns = NULL;
    else
      {
	  len = strlen (tag_ns);
	  p->tag_ns = malloc (len + 1);
	  strcpy (p->tag_ns, tag_ns);
      }
    len = strlen (tag_name);
    p->tag_name = malloc (len + 1);
    strcpy (p->tag_name, tag_name);
    if (geometry == NULL)
	p->geometry = NULL;
    else
      {
	  len = strlen (geometry);
	  p->geometry = malloc (len + 1);
	  strcpy (p->geometry, geometry);
      }
    p->parent = NULL;
    p->first = NULL;
    p->last = NULL;
    p->done = 0;
    p->level = level;
    p->next = NULL;
    return p;
}

static void
destroy_xml_table (struct xml_table *p)
{
/* memory cleanup - destroying an XML table */
    struct child_reference *pc;
    struct child_reference *pcn;
    if (p == NULL)
	return;
    if (p->table_name)
	free (p->table_name);
    if (p->parent_table)
	free (p->parent_table);
    if (p->tag_ns)
	free (p->tag_ns);
    if (p->tag_name)
	free (p->tag_name);
    if (p->geometry)
	free (p->geometry);
    pc = p->first;
    while (pc != NULL)
      {
	  pcn = pc->next;
	  destroy_child_reference (pc);
	  pc = pcn;
      }
    free (p);
}

static struct xml_geometry *
alloc_xml_geometry (const char *table, const char *geometry)
{
/* allocating and initializing an XML geometry */
    int len;
    struct xml_geometry *p = malloc (sizeof (struct xml_geometry));
    len = strlen (table);
    p->table_name = malloc (len + 1);
    strcpy (p->table_name, table);
    len = strlen (geometry);
    p->geometry = malloc (len + 1);
    strcpy (p->geometry, geometry);
    p->type = NULL;
    p->srid = -1;
    p->dims = NULL;
    p->next = NULL;
    return p;
}

static void
destroy_xml_geometry (struct xml_geometry *p)
{
/* memory cleanup - destroying an XML Geometry */
    if (p == NULL)
	return;
    if (p->table_name)
	free (p->table_name);
    if (p->geometry)
	free (p->geometry);
    if (p->type)
	free (p->type);
    if (p->dims)
	free (p->dims);
    free (p);
}

static struct xml_tables_list *
alloc_xml_tables (void)
{
/* creating an empty list */
    struct xml_tables_list *p = malloc (sizeof (struct xml_tables_list));
    p->first = NULL;
    p->last = NULL;
    p->first_geom = NULL;
    p->last_geom = NULL;
    return p;
}

static void
destroy_xml_tables (struct xml_tables_list *p)
{
/* memory cleanup - destroying the list */
    struct xml_table *pt;
    struct xml_table *ptn;
    struct xml_geometry *pg;
    struct xml_geometry *pgn;
    if (p == NULL)
	return;
    pt = p->first;
    while (pt != NULL)
      {
	  ptn = pt->next;
	  destroy_xml_table (pt);
	  pt = ptn;
      }
    pg = p->first_geom;
    while (pg != NULL)
      {
	  pgn = pg->next;
	  destroy_xml_geometry (pg);
	  pg = pgn;
      }
    free (p);
}

static void
add_xml_table (struct xml_tables_list *list, const char *table,
	       const char *parent, const char *tag_ns, const char *tag_name,
	       const char *geometry, int level)
{
/* adds an XML table into the list */
    struct xml_table *p =
	alloc_xml_table (table, parent, tag_ns, tag_name, geometry, level);
    if (list->first == NULL)
	list->first = p;
    if (list->last != NULL)
	list->last->next = p;
    list->last = p;
}

static void
add_xml_geometry (struct xml_tables_list *list, const char *table,
		  const char *geometry)
{
/* adds an XML geometry into the list */
    struct xml_geometry *p = alloc_xml_geometry (table, geometry);
    if (list->first_geom == NULL)
	list->first_geom = p;
    if (list->last_geom != NULL)
	list->last_geom->next = p;
    list->last_geom = p;
}

static void
add_child (struct xml_tables_list *list, struct xml_table *child,
	   const char *parent)
{
/* inserting a child into the parent */
    struct xml_table *p = list->first;
    while (p != NULL)
      {
	  if (strcasecmp (p->table_name, parent) == 0)
	    {
		struct child_reference *ref =
		    malloc (sizeof (struct child_reference));
		ref->child = child;
		ref->next = NULL;
		if (p->first == NULL)
		    p->first = ref;
		if (p->last != NULL)
		    p->last->next = ref;
		p->last = ref;
		child->parent = p;
		return;
	    }
	  p = p->next;
      }
}

static void
identify_childs (struct xml_tables_list *list)
{
/* identifying parent-child relationships */
    struct xml_table *p = list->first;
    while (p != NULL)
      {
	  if (p->parent_table != NULL)
	      add_child (list, p, p->parent_table);
	  p = p->next;
      }
}

static void
add_attribute (struct new_attributes *p, const char *name, int main)
{
/* adding a new XML Attribute (column) into the container */
    int len;
    int i;
    struct xml_attribute *attr = malloc (sizeof (struct xml_attribute));
    len = strlen (name);
    attr->attr_name = malloc (len + 1);
    strcpy (attr->attr_name, name);
    for (i = 0; i < len; i++)
      {
	  char c = *(attr->attr_name + i);
	  if (c >= 'A' && c <= 'Z')
	      *(attr->attr_name + i) = c - 'A' + 'a';
      }
    attr->main_collapsed = main;
    attr->already_defined = 0;
    attr->datatype = SQLITE_NULL;
    attr->xml_reference = NULL;
    attr->next = NULL;
    if (p->first == NULL)
	p->first = attr;
    if (p->last != NULL)
	p->last->next = attr;
    p->last = attr;
}

static void
destroy_attribute (struct xml_attribute *attr)
{
/* memory cleanup: destroying an XML Attribute (column) */
    if (attr == NULL)
	return;
    if (attr->attr_name != NULL)
	free (attr->attr_name);
    if (attr->xml_reference != NULL)
	free (attr->xml_reference);
    free (attr);
}

static struct new_attributes *
alloc_new_attributes (void)
{
/* creating an empty container */
    struct new_attributes *p = malloc (sizeof (struct new_attributes));
    p->first = NULL;
    p->last = NULL;
    p->collision = 0;
    return p;
}

static void
destroy_new_attributes (struct new_attributes *p)
{
/* memory cleanup: destroying a container */
    struct xml_attribute *pa;
    struct xml_attribute *pan;
    if (p == NULL)
	return;
    pa = p->first;
    while (pa != NULL)
      {
	  pan = pa->next;
	  destroy_attribute (pa);
	  pa = pan;
      }
    free (p);
}

static void
check_attribute (struct new_attributes *p, const char *name)
{
/* checks if an Attribute/Column name is already defined */
    struct xml_attribute *attr = p->first;
    while (attr != NULL)
      {
	  if (strcasecmp (attr->attr_name, name) == 0)
	    {
		attr->already_defined = 1;
		p->collision = 1;
	    }
	  attr = attr->next;
      }
}

static void
reset_attribute_values (struct new_attributes *p)
{
/* resetting all values as NULL */
    struct xml_attribute *attr = p->first;
    while (attr != NULL)
      {
	  attr->datatype = SQLITE_NULL;
	  attr = attr->next;
      }
}

static void
set_int_value (struct new_attributes *p, const char *name,
	       sqlite3_int64 int_value)
{
/* setting an INT-64 attribute value */
    struct xml_attribute *attr = p->first;
    while (attr != NULL)
      {
	  if (strcasecmp (attr->attr_name, name) == 0)
	    {
		attr->datatype = SQLITE_INTEGER;
		attr->int_value = int_value;
		break;
	    }
	  attr = attr->next;
      }
}

static void
set_double_value (struct new_attributes *p, const char *name,
		  double double_value)
{
/* setting a FLOATING POINT (DOUBLE) attribute value */
    struct xml_attribute *attr = p->first;
    while (attr != NULL)
      {
	  if (strcasecmp (attr->attr_name, name) == 0)
	    {
		attr->datatype = SQLITE_FLOAT;
		attr->double_value = double_value;
		break;
	    }
	  attr = attr->next;
      }
}

static void
set_text_value (struct new_attributes *p, const char *name,
		const unsigned char *text_value)
{
/* setting a TEXT attribute value */
    struct xml_attribute *attr = p->first;
    while (attr != NULL)
      {
	  if (strcasecmp (attr->attr_name, name) == 0)
	    {
		attr->datatype = SQLITE_TEXT;
		attr->text_value = text_value;
		break;
	    }
	  attr = attr->next;
      }
}

static void
set_blob_value (struct new_attributes *p, const char *name,
		const unsigned char *blob_value, int blob_size)
{
/* setting a BLOB attribute value */
    struct xml_attribute *attr = p->first;
    while (attr != NULL)
      {
	  if (strcasecmp (attr->attr_name, name) == 0)
	    {
		attr->datatype = SQLITE_BLOB;
		attr->blob_value = blob_value;
		attr->blob_size = blob_size;
		break;
	    }
	  attr = attr->next;
      }
}

static char *
get_main_attribute (struct new_attributes *p)
{
/* return the name of the "main" attribute */
    struct xml_attribute *attr = p->first;
    while (attr != NULL)
      {
	  if (attr->main_collapsed == 1)
	      return attr->attr_name;
	  attr = attr->next;
      }
    return NULL;
}

static int
do_delete_duplicates2 (sqlite3 * sqlite, sqlite3_int64 rowid,
		       sqlite3_stmt * stmt1, int *count)
{
/* deleting duplicate rows [actual delete] */
    int ret;

    *count = 0;
    sqlite3_reset (stmt1);
    sqlite3_clear_bindings (stmt1);
    sqlite3_bind_int64 (stmt1, 1, rowid);
    ret = sqlite3_step (stmt1);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
	  goto error;
      }
    *count = 1;
    return 1;

  error:
    *count = 0;

    return 0;
}

static void
do_delete_duplicates (sqlite3 * sqlite, const char *sql,
		      const char *sql_del, int *count)
{
/* deleting duplicate rows */
    sqlite3_stmt *stmt1 = NULL;
    sqlite3_stmt *stmt2 = NULL;
    int ret;
    int xcnt;
    int cnt = 0;
    char *sql_err = NULL;
    struct resultset_comparator *rs_obj = NULL;

    *count = 0;

/* the complete operation is handled as an unique SQL Transaction */
    ret = sqlite3_exec (sqlite, "BEGIN", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }
/* preparing the main SELECT statement */
    ret = sqlite3_prepare_v2 (sqlite, sql, strlen (sql), &stmt1, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
	  return;
      }
/* preparing the DELETE statement */
    ret = sqlite3_prepare_v2 (sqlite, sql_del, strlen (sql_del), &stmt2, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
	  goto error;
      }

    rs_obj = create_resultset_comparator (sqlite3_column_count (stmt1) - 1);
    while (1)
      {
	  /* fetching the result set rows */
	  ret = sqlite3_step (stmt1);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* fetching a row */
		save_row_from_resultset (rs_obj, stmt1);
		if (resultset_rows_equals (rs_obj))
		  {
		      if (do_delete_duplicates2
			  (sqlite, get_current_resultset_rowid (rs_obj), stmt2,
			   &xcnt))
			{
			    cnt += xcnt;
			    reset_resultset_current_row (rs_obj);
			    continue;
			}
		      else
			  goto error;
		  }
		swap_resultset_rows (rs_obj);
	    }
	  else
	    {
		fprintf (stderr, "SQL error: %s\n", sqlite3_errmsg (sqlite));
		goto error;
	    }
      }
    destroy_resultset_comparator (rs_obj);

    sqlite3_finalize (stmt1);
    sqlite3_finalize (stmt2);

/* confirm the still pending Transaction */
    ret = sqlite3_exec (sqlite, "COMMIT", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
	  return;
      }

    *count = cnt;
    return;

  error:
    *count = 0;
    if (stmt1)
	sqlite3_finalize (stmt1);
    if (stmt2)
	sqlite3_finalize (stmt2);
    if (rs_obj != NULL)
	destroy_resultset_comparator (rs_obj);

/* performing a ROLLBACK anyway */
    ret = sqlite3_exec (sqlite, "ROLLBACK", NULL, NULL, &sql_err);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "ROLLBACK TRANSACTION error: %s\n", sql_err);
	  sqlite3_free (sql_err);
      }
}

static char *
eval_table (sqlite3 * db_handle, char *table)
{
/* preparing the column list for Delete Duplicates */
    int ret;
    char *sql;
    char **results;
    int rows;
    int columns;
    int i;
    char *list = NULL;

/* testing if the column/table really exists */
    sql = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", table);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
	goto done;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		if (strcmp (name, "node_id") == 0)
		    continue;
		if (strcmp (name, "parent_id") == 0)
		    continue;
		if (list == NULL)
		    list = sqlite3_mprintf ("\"%s\"", name);
		else
		  {
		      char *tmp = sqlite3_mprintf ("%s, \"%s\"", list, name);
		      sqlite3_free (list);
		      list = tmp;
		  }
	    }
      }
    sqlite3_free_table (results);
  done:
    return list;
}

static void
remove_duplicates (sqlite3 * db_handle, struct xml_table *table)
{
/* attempting to delete Duplicated rows from a table */
    int count;
    char *sql;
    char *sql_del;
    char *column_list = eval_table (db_handle, table->table_name);
    if (column_list == NULL)
      {
	  table->done = 1;
	  return;
      }
    sql = sqlite3_mprintf ("SELECT ROWID, %s FROM \"%s\" ORDER BY %s, ROWID",
			   column_list, table->table_name, column_list);
    sqlite3_free (column_list);
    sql_del =
	sqlite3_mprintf ("DELETE FROM \"%s\" WHERE ROWID = ?",
			 table->table_name);
    do_delete_duplicates (db_handle, sql, sql_del, &count);
    if (!count)
	printf ("No duplicated rows found in: %s\n", table->table_name);
    else
	printf ("%9d duplicated rows deleted from: %s\n", count,
		table->table_name);
    fflush (stdout);
    sqlite3_free (sql);
    sqlite3_free (sql_del);
    table->done = 1;
}

static void
recover_geometry (sqlite3 * db_handle, struct xml_geometry *geom)
{
/* recovering a full Geometry Column */
    int ret;
    char *err_msg;
    char *sql;

    printf ("Recovering Geometry:    %s.%s\n", geom->table_name,
	    geom->geometry);
    fflush (stdout);

    sql = sqlite3_mprintf ("SELECT RecoverGeometryColumn(%Q, %Q, %d, %Q, %Q)",
			   geom->table_name, geom->geometry, geom->srid,
			   geom->type, geom->dims);
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "RecoverGeometryColumn error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return;
      }

    fprintf (stderr, "Creating Spatial Index: %s.%s\n", geom->table_name,
	     geom->geometry);

    sql = sqlite3_mprintf ("SELECT CreateSpatialIndex(%Q, %Q)",
			   geom->table_name, geom->geometry);
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CreateSpatialIndex error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return;
      }
}

static void
check_geometry (sqlite3 * db_handle, struct xml_geometry *geom)
{
/* checks if a geometry could be recovered */
    int ret;
    char *sql;
    char **results;
    int rows;
    int columns;
    int i;
    int exists = 0;
    int registered = 0;

/* testing if the column/table really exists */
    sql = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", geom->table_name);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
	goto done1;
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		if (strcmp (name, geom->geometry) == 0)
		    exists = 1;
	    }
      }
    sqlite3_free_table (results);
  done1:

/* testing if the geometry isn't yet registered */
    sql = sqlite3_mprintf ("SELECT f_geometry_column FROM geometry_columns "
			   "WHERE Lower(f_table_name) = Lower(%Q) AND "
			   "Lower(f_geometry_column) = Lower(%Q)",
			   geom->table_name, geom->geometry);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  registered = 1;
	  goto done2;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 0];
		if (strcmp (name, geom->geometry) == 0)
		    registered = 1;
	    }
      }
    sqlite3_free_table (results);
  done2:
    if (exists && !registered)
	;
    else
	return;

/* testing if the geometry could really be recovered */
    sql = sqlite3_mprintf ("SELECT DISTINCT ST_GeometryType(\"%s\"), "
			   "ST_Srid(\"%s\"), CoordDimension(\"%s\") "
			   "FROM \"%s\"", geom->geometry, geom->geometry,
			   geom->geometry, geom->table_name);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
	return;
    if (rows != 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		int len;
		int i2;
		const char *type = results[(i * columns) + 0];
		const char *srid = results[(i * columns) + 1];
		const char *dims = results[(i * columns) + 2];
		if (geom->type != NULL)
		    free (geom->type);
		len = strlen (type);
		geom->type = malloc (len + 1);
		strcpy (geom->type, type);
		for (i2 = 0; i2 < len; i2++)
		  {
		      if (*(geom->type + i2) == ' ')
			  *(geom->type + i2) = '\0';
		  }
		geom->srid = atoi (srid);
		if (geom->dims != NULL)
		    free (geom->dims);
		len = strlen (dims);
		geom->dims = malloc (len + 1);
		strcpy (geom->dims, dims);
	    }
      }
    sqlite3_free_table (results);
}

static int
collapse_table (sqlite3 * db_handle, int journal_off,
		struct new_attributes *list, const char *table,
		const char *parent, struct xml_table *tbl)
{
/* attempting to collapse some Child table into the Parent */
    int ret;
    char *err_msg = NULL;
    char *sql;
    char *sql1;
    struct xml_attribute *attr;
    sqlite3_stmt *sel_stmt = NULL;
    sqlite3_stmt *upd_stmt = NULL;
    int error = 0;
    sqlite3_int64 pk_value;
    const unsigned char *main_value;
    int i;
    char *tag_child;
    char *tag_parent;

    if (tbl->tag_ns == NULL)
	tag_child = sqlite3_mprintf ("<%s>", tbl->tag_name);
    else
	tag_child = sqlite3_mprintf ("<%s:%s>", tbl->tag_ns, tbl->tag_name);
    if (tbl->parent->tag_ns == NULL)
	tag_parent = sqlite3_mprintf ("<%s>", tbl->parent->tag_name);
    else
	tag_parent =
	    sqlite3_mprintf ("<%s:%s>", tbl->parent->tag_ns,
			     tbl->parent->tag_name);
    printf ("Collapsing %s%s\n", tag_parent, tag_child);
    fflush (stdout);
    sqlite3_free (tag_child);
    sqlite3_free (tag_parent);

    sql = sqlite3_mprintf ("SELECT parent_id, node_value");
    attr = list->first;
    while (attr != NULL)
      {
	  if (attr->main_collapsed)
	    {
		/* skipping the "main" attribute (child tag name) */
		attr = attr->next;
		continue;
	    }
	  sql1 =
	      sqlite3_mprintf ("%s, \"%s\" AS \"%s\"", sql, attr->attr_name,
			       attr->attr_name);
	  sqlite3_free (sql);
	  sql = sql1;
	  attr = attr->next;
      }
    sql1 = sqlite3_mprintf ("%s FROM \"%s\"", sql, table);
    sqlite3_free (sql);
    sql = sql1;
    ret = sqlite3_prepare_v2 (db_handle, sql, strlen (sql), &sel_stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT FROM Child error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  error = 1;
	  goto stop;
      }

    sql =
	sqlite3_mprintf ("UPDATE \"%s\" SET \"%s\" = ?", parent,
			 get_main_attribute (list));
    attr = list->first;
    while (attr != NULL)
      {
	  if (attr->main_collapsed)
	    {
		/* skipping the "main" attribute (child tag name) */
		attr = attr->next;
		continue;
	    }
	  sql1 = sqlite3_mprintf ("%s, \"%s\" = ?", sql, attr->attr_name);
	  sqlite3_free (sql);
	  sql = sql1;
	  attr = attr->next;
      }
    sql1 = sqlite3_mprintf ("%s WHERE node_id = ?", sql);
    sqlite3_free (sql);
    sql = sql1;
    ret = sqlite3_prepare_v2 (db_handle, sql, strlen (sql), &upd_stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE Parent error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  error = 1;
	  goto stop;
      }

    if (journal_off)
      {
	  /* disabling the Journal File */
	  ret =
	      sqlite3_exec (db_handle, "PRAGMA journal_mode=OFF", NULL,
			    NULL, &err_msg);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "JOURNAL MODE=OFF error: %s\n", err_msg);
		sqlite3_free (err_msg);
		goto stop;
	    }
	  ret =
	      sqlite3_exec (db_handle, "PRAGMA synchronous=OFF", NULL,
			    NULL, &err_msg);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "SYNCHRONOUS=OFF error: %s\n", err_msg);
		sqlite3_free (err_msg);
		goto stop;
	    }
      }

/* starting a transaction */
    ret = sqlite3_exec (db_handle, "BEGIN", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto stop;
      }

    while (1)
      {
	  /* scrolling the result set rows */
	  ret = sqlite3_step (sel_stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	    {
		/* fetching an input row from the Child Table */
		int main_value_null = 0;
		pk_value = sqlite3_column_int64 (sel_stmt, 0);
		if (sqlite3_column_type (sel_stmt, 1) == SQLITE_NULL)
		    main_value_null = 1;
		else
		    main_value = sqlite3_column_text (sel_stmt, 1);
		reset_attribute_values (list);
		for (i = 2; i < sqlite3_column_count (sel_stmt); i++)
		  {
		      const char *name = sqlite3_column_name (sel_stmt, i);
		      switch (sqlite3_column_type (sel_stmt, i))
			{
			case SQLITE_INTEGER:
			    set_int_value (list, name,
					   sqlite3_column_int64 (sel_stmt, i));
			    break;
			case SQLITE_FLOAT:
			    set_double_value (list, name,
					      sqlite3_column_double (sel_stmt,
								     i));
			    break;
			case SQLITE_TEXT:
			    set_text_value (list, name,
					    sqlite3_column_text (sel_stmt, i));
			    break;
			case SQLITE_BLOB:
			    set_blob_value (list, name,
					    sqlite3_column_blob (sel_stmt, i),
					    sqlite3_column_bytes (sel_stmt, i));
			    break;
			};
		  }
		/* updating the Parent Table */
		sqlite3_reset (upd_stmt);
		sqlite3_clear_bindings (upd_stmt);
		if (main_value_null)
		    sqlite3_bind_null (upd_stmt, 1);
		else
		    sqlite3_bind_text (upd_stmt, 1, (const char *) main_value,
				       strlen ((const char *) main_value),
				       SQLITE_STATIC);
		i = 2;
		attr = list->first;
		while (attr != NULL)
		  {
		      if (attr->main_collapsed)
			{
			    /* skipping the "main" attribute (child tag name) */
			    attr = attr->next;
			    continue;
			}
		      switch (attr->datatype)
			{
			case SQLITE_INTEGER:
			    sqlite3_bind_int64 (upd_stmt, i, attr->int_value);
			    break;
			case SQLITE_FLOAT:
			    sqlite3_bind_double (upd_stmt, i,
						 attr->double_value);
			    break;
			case SQLITE_TEXT:
			    sqlite3_bind_text (upd_stmt, i,
					       (const char
						*) (attr->text_value),
					       strlen ((const char
							*) (attr->text_value)),
					       SQLITE_STATIC);
			    break;
			case SQLITE_BLOB:
			    sqlite3_bind_blob (upd_stmt, i, attr->blob_value,
					       attr->blob_size, SQLITE_STATIC);
			    break;
			default:
			    sqlite3_bind_null (upd_stmt, i);
			    break;
			};
		      i++;
		      attr = attr->next;
		  }
		sqlite3_bind_int64 (upd_stmt, i, pk_value);
		/* UPDATing */
		ret = sqlite3_step (upd_stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr, "sqlite3_step [WRITE] error: %s\n",
			       sqlite3_errmsg (db_handle));
		      error = 1;
		      goto stop;
		  }
	    }
	  else
	    {
		fprintf (stderr, "sqlite3_step [READ] error: %s\n",
			 sqlite3_errmsg (db_handle));
		error = 1;
		goto stop;
	    }
      }

/* finally dropping the Child Table */
    sql = sqlite3_mprintf ("DROP TABLE \"%s\"", table);
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "DROP TABLE error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  error = 1;
	  goto stop;
      }

/* updating xml_metacatalog_tables (child) */
    sql = sqlite3_mprintf ("UPDATE xml_metacatalog_tables "
			   "SET status = 'post-processed: collapsed and then dropped' "
			   "WHERE table_name = %Q", tbl->table_name);
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE xml_metacatalog_tables (child): %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
      }

/* updating xml_metacatalog_tables (parent) */
    sql = sqlite3_mprintf ("UPDATE xml_metacatalog_tables "
			   "SET status = 'post-processed: receiving collapsed children' "
			   "WHERE table_name = %Q", tbl->parent_table);
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE xml_metacatalog_tables (parent): %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
      }

    attr = list->first;
    while (attr != NULL)
      {
	  /* updating xml_metacatalog_columns (parent) */
	  char *xml;
	  char *parent_tag;
	  char *geom = NULL;
	  if (tbl->parent->tag_ns == NULL)
	      parent_tag = sqlite3_mprintf ("<%s>", tbl->parent->tag_name);
	  else
	      parent_tag =
		  sqlite3_mprintf ("<%s:%s>", tbl->parent->tag_ns,
				   tbl->parent->tag_name);
	  xml =
	      sqlite3_mprintf ("%s%s%s", parent_tag, attr->xml_reference,
			       parent_tag);
	  sqlite3_free (parent_tag);
	  if (tbl->geometry != NULL)
	    {
		if (strcmp (attr->attr_name, tbl->geometry) == 0)
		    geom =
			sqlite3_mprintf ("GeomFromGml(\"%s\")", tbl->tag_name);
	    }
	  if (geom != NULL)
	    {
		sql = sqlite3_mprintf ("INSERT INTO xml_metacatalog_columns "
				       "(table_name, column_name, origin, destination, xml_reference) VALUES "
				       "(%Q, %Q, %Q, NULL, %Q)",
				       tbl->parent_table, attr->attr_name, geom,
				       xml);
		sqlite3_free (geom);
	    }
	  else
	      sql = sqlite3_mprintf ("INSERT INTO xml_metacatalog_columns "
				     "(table_name, column_name, origin, destination, xml_reference) VALUES "
				     "(%Q, %Q, %Q, NULL, %Q)",
				     tbl->parent_table, attr->attr_name,
				     "collapsed from child node", xml);
	  sqlite3_free (xml);
	  ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "INSERT INTO xml_metacatalog_columns: %s\n",
			 err_msg);
		sqlite3_free (err_msg);
	    }
	  /* updating xml_metacatalog_columns (child) */
	  xml =
	      sqlite3_mprintf ("collapsed into \"%s\".\"%s\"",
			       tbl->parent_table, attr->attr_name);
	  sql =
	      sqlite3_mprintf
	      ("UPDATE xml_metacatalog_columns SET destination = %Q "
	       "WHERE table_name = %Q AND column_name = %Q", xml,
	       tbl->table_name, attr->attr_name);
	  sqlite3_free (xml);
	  ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "UPDATE xml_metacatalog_columns: %s\n",
			 err_msg);
		sqlite3_free (err_msg);
	    }
	  attr = attr->next;
      }

/* committing the transaction */
    ret = sqlite3_exec (db_handle, "COMMIT", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT error: %s\n", err_msg);
	  sqlite3_free (err_msg);
      }

  stop:
    if (sel_stmt != NULL)
	sqlite3_finalize (sel_stmt);
    if (upd_stmt != NULL)
	sqlite3_finalize (upd_stmt);
    if (error)
	return 0;
    return 1;
}

static void
get_xml_references (sqlite3 * db_handle, struct new_attributes *list,
		    const char *table)
{
/* fetching the xml_references for any column to be collapsed */
    struct xml_attribute *attr = list->first;
    while (attr != NULL)
      {
	  int ret;
	  char *err_msg = NULL;
	  char **results;
	  int rows;
	  int columns;
	  int i;
	  int len;
	  const char *name;
	  char *sql;
	  if (attr->main_collapsed)
	      sql = sqlite3_mprintf ("SELECT xml_reference "
				     "FROM xml_metacatalog_columns "
				     "WHERE table_name = %Q AND column_name = %Q",
				     table, "node_value");
	  else
	      sql = sqlite3_mprintf ("SELECT xml_reference "
				     "FROM xml_metacatalog_columns "
				     "WHERE table_name = %Q AND column_name = %Q",
				     table, attr->attr_name);
	  ret =
	      sqlite3_get_table (db_handle, sql, &results, &rows, &columns,
				 NULL);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "SELECT xml_referemce error: %s\n", err_msg);
		sqlite3_free (err_msg);
		goto default_ref;
	    }
	  if (rows < 1)
	    {
		sqlite3_free_table (results);
		goto default_ref;
	    }
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 0];
		if (attr->xml_reference != NULL)
		    free (attr->xml_reference);
		len = strlen (name);
		attr->xml_reference = malloc (len + 1);
		strcpy (attr->xml_reference, name);
	    }
	  sqlite3_free_table (results);
	  attr = attr->next;
	  continue;
	default_ref:
	  name = "?unknown?";
	  if (attr->xml_reference != NULL)
	      free (attr->xml_reference);
	  len = strlen (name);
	  attr->xml_reference = malloc (len + 1);
	  strcpy (attr->xml_reference, name);
	  attr = attr->next;
      }
}

static int
test_index (sqlite3 * db_handle, const char *table, const char *column)
{
/* tests if an index exists */
    int ret;
    char *err_msg = NULL;
    char **results;
    int rows;
    int columns;
    int i;
    int ok = 0;

/* checking the expected columns */
    char *idx = sqlite3_mprintf ("idx_%s_%s", table, column);
    char *sql = sqlite3_mprintf ("SELECT name FROM sqlite_master "
				 "WHERE type = 'index' AND tbl_name = %Q AND name = %Q",
				 table, idx);
    sqlite3_free (idx);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT FROM sqlite_master error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	      ok = 1;
      }
    sqlite3_free_table (results);
    return ok;
}

static struct new_attributes *
upgrade_parent (sqlite3 * db_handle, const char *table, const char *parent,
		const char *attr_name, const char *geometry_name)
{
/* attempting to upgrade the parent table */
    int ret;
    char *err_msg = NULL;
    char **results;
    int rows;
    int columns;
    int i;
    int error = 0;
    struct new_attributes *list = alloc_new_attributes ();
    char *sql;
    struct xml_attribute *attr;

/* inserting the tag name itself */
    add_attribute (list, attr_name, 1);

/* extracting the column (XML Attributes) names from the Child Table */
    sql = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", table);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA table_info[Child] error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto done;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		if (strcmp (name, "node_id") == 0)
		    continue;
		if (strcmp (name, "parent_id") == 0)
		    continue;
		if (strcmp (name, "node_value") == 0)
		    continue;
		add_attribute (list, name, 0);
	    }
      }
    sqlite3_free_table (results);

/* extracting the column (XML Attributes) names from the Parent Table */
    sql = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", parent);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA table_info[Parent] error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto done;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		check_attribute (list, name);
	    }
      }
    sqlite3_free_table (results);

    if (list->collision)
      {
	  error = 1;
	  goto done;
      }

    attr = list->first;
    while (attr != NULL)
      {
	  /* adding the new Columns to the Parent Table */
	  const char *type = "TEXT";
	  if (geometry_name != NULL)
	    {
		if (strcmp (geometry_name, attr->attr_name) == 0)
		    type = "BLOB";
	    }
	  sql =
	      sqlite3_mprintf ("ALTER TABLE \"%s\" ADD COLUMN \"%s\" %s",
			       parent, attr->attr_name, type);
	  ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "ALTER TABLE error: %s\n", err_msg);
		sqlite3_free (err_msg);
		error = 1;
		goto done;
	    }
	  attr = attr->next;
      }

    attr = list->first;
    while (attr != NULL)
      {
	  if (strcmp (attr->attr_name, "gml_id") == 0)
	    {
		if (test_index (db_handle, table, "gml_id"))
		  {
		      /* creating an Index supporting "gml_id" */
		      sql = sqlite3_mprintf ("CREATE INDEX IF NOT EXISTS "
					     "\"idx_%s_gml_id\" ON \"%s\" (gml_id)",
					     parent, parent);
		      ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
		      sqlite3_free (sql);
		      if (ret != SQLITE_OK)
			{
			    fprintf (stderr,
				     "CREATE INDEX [gml_id] error: %s\n",
				     err_msg);
			    sqlite3_free (err_msg);
			    error = 1;
			    goto done;
			}
		  }
	    }
	  if (strcmp (attr->attr_name, "xlink_href") == 0)
	    {
		if (test_index (db_handle, table, "xlink_href"))
		  {
		      /* creating an Index supporting "xlink_href" */
		      sql = sqlite3_mprintf ("CREATE INDEX IF NOT EXISTS "
					     "\"idx_%s_xlink_href\" ON \"%s\" (xlink_href)",
					     parent, parent);
		      ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
		      sqlite3_free (sql);
		      if (ret != SQLITE_OK)
			{
			    fprintf (stderr,
				     "CREATE INDEX [xlink_href] error: %s\n",
				     err_msg);
			    sqlite3_free (err_msg);
			    error = 1;
			    goto done;
			}
		  }
	    }
	  attr = attr->next;
      }

  done:
    if (error)
      {
	  destroy_new_attributes (list);
	  return NULL;
      }
    return list;
}

static int
check_collapsible (sqlite3 * db_handle, const char *table, const char *parent)
{
/* tests if this table could be collapsed as an XML parent Attribute */
    char *sql;
    int ret;
    char *err_msg = NULL;
    int multi = 0;
    sqlite3_stmt *stmt;

    sql = sqlite3_mprintf ("SELECT p.node_id, Count(*) AS cnt "
			   "FROM \"%s\" AS c JOIN \"%s\" AS p ON (p.node_id = c.parent_id) "
			   "GROUP BY p.node_id HAVING cnt > 1", table, parent);
    ret = sqlite3_prepare_v2 (db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT multi-values: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    while (1)
      {
	  /* scrolling the result set rows */
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;		/* end of result set */
	  if (ret == SQLITE_ROW)
	      multi++;
      }
    sqlite3_finalize (stmt);
    if (multi > 0)
	return 0;

    return 1;
}

static char *
check_xml_child_table (sqlite3 * db_handle, const char *table)
{
/* tests if this table seems to be an XML child table */
    int ret;
    char *err_msg = NULL;
    char **results;
    int rows;
    int columns;
    int i;
    int ok_pk = 0;
    int ok_fk = 0;
    int ok_value = 0;
    int error = 0;
    char *parent = NULL;
    int fk = 0;

/* checking the expected columns */
    char *sql = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", table);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA table_info error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return NULL;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		if (strcmp (name, "node_id") == 0)
		  {
		      if (atoi (results[(i * columns) + 5]) == 1)
			  ok_pk = 1;
		  }
		else if (atoi (results[(i * columns) + 5]) == 1)
		    error = 1;
		if (strcmp (name, "parent_id") == 0)
		    ok_fk = 1;
		if (strcmp (name, "node_value") == 0)
		    ok_value = 1;
	    }
      }
    sqlite3_free_table (results);
    if (error)
	return NULL;
    if (ok_pk == 0)
	return NULL;
    if (ok_fk == 0)
	return NULL;
    if (ok_value == 0)
	return NULL;

/* checking the Foreing Key */
    sql = sqlite3_mprintf ("PRAGMA foreign_key_list(\"%s\")", table);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA foreign_key_list error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return NULL;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		int len;
		const char *name = results[(i * columns) + 2];
		if (parent != NULL)
		    free (parent);
		len = strlen (name);
		parent = malloc (len + 1);
		strcpy (parent, name);
		fk++;
		if (strcmp (results[(i * columns) + 3], "parent_id") != 0
		    || strcmp (results[(i * columns) + 4], "node_id") != 0)
		    error = 1;
	    }
      }
    sqlite3_free_table (results);
    if (error || fk != 1)
      {
	  if (parent != NULL)
	      free (parent);
	  return NULL;
      }

    return parent;
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
check_xml_metacatalog (sqlite3 * db_handle)
{
/* checking if XML-metacatalog tables do really exist */
    int ret;
    const char *sql;
    char *err_msg = NULL;
    char **results;
    int rows;
    int columns;
    int i;
    int ok_table_name = 0;
    int ok_tree_level = 0;
    int ok_xml_tag_namespace = 0;
    int ok_xml_tag_name = 0;
    int ok_parent_table_name = 0;
    int ok_gml_geometry_column = 0;
    int ok_status = 0;
    int ok_column_name = 0;
    int ok_origin = 0;
    int ok_destination = 0;
    int ok_xml_reference = 0;
    int error = 0;

/* checking xml_metacatalog_tables */
    sql = "PRAGMA table_info(xml_metacatalog_tables)";
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA xml_metacatalog_tables error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  goto done;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		if (strcmp (name, "table_name") == 0)
		    ok_table_name = 1;
		if (strcmp (name, "tree_level") == 0)
		    ok_tree_level = 1;
		if (strcmp (name, "xml_tag_namespace") == 0)
		    ok_xml_tag_namespace = 1;
		if (strcmp (name, "xml_tag_name") == 0)
		    ok_xml_tag_name = 1;
		if (strcmp (name, "parent_table_name") == 0)
		    ok_parent_table_name = 1;
		if (strcmp (name, "gml_geometry_column") == 0)
		    ok_gml_geometry_column = 1;
		if (strcmp (name, "status") == 0)
		    ok_status = 1;
	    }
      }
    sqlite3_free_table (results);
    if (ok_table_name && ok_tree_level && ok_xml_tag_namespace
	&& ok_xml_tag_name && ok_parent_table_name && ok_gml_geometry_column
	&& ok_status)
	;
    else
      {
	  error = 1;
	  goto done;
      }
    ok_table_name = 0;

/* checking xml_metacatalog_columns */
    sql = "PRAGMA table_info(xml_metacatalog_columns)";
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA xml_metacatalog_columns error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  goto done;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		if (strcmp (name, "table_name") == 0)
		    ok_table_name = 1;
		if (strcmp (name, "column_name") == 0)
		    ok_column_name = 1;
		if (strcmp (name, "origin") == 0)
		    ok_origin = 1;
		if (strcmp (name, "destination") == 0)
		    ok_destination = 1;
		if (strcmp (name, "xml_reference") == 0)
		    ok_xml_reference = 1;
	    }
      }
    sqlite3_free_table (results);
    if (ok_table_name && ok_column_name && ok_origin && ok_destination
	&& ok_xml_reference)
	;
    else
      {
	  error = 1;
	  goto done;
      }

  done:
    if (error)
	return 0;
    return 1;
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

    if (!check_xml_metacatalog (db_handle))
      {
	  fprintf (stderr,
		   "XML-metacatalog not found or invalid ... cowardly quitting\n");
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

/* enabling PK/FK constraints */
    sqlite3_exec (db_handle, "PRAGMA foreign_keys = 1", NULL, NULL, NULL);
    *handle = db_handle;
    return;
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_xml_collapse ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr,
	     "-d or --db-path     pathname    the SpatiaLite DB path\n\n");
    fprintf (stderr, "you can specify the following options as well\n");
    fprintf (stderr,
	     "-dd or --delete-duplicates      remove all duplicate rows except one\n");
    fprintf (stderr,
	     "-nl or --nl-level      num      tree-level for table-names (dft: 0)\n\n");
    fprintf (stderr,
	     "-jo or --journal-off            unsafe [but faster] mode\n");
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
    char *err_msg = NULL;
    int ret;
    int i;
    int error = 0;
    int next_arg = ARG_NONE;
    const char *db_path = NULL;
    int delete_duplicates = 0;
    int name_level = -1;
    int in_memory = 0;
    int cache_size = 0;
    int journal_off = 0;
    void *cache;
    const char *sql;
    char **results;
    int rows;
    int columns;
    int loop_again = 1;
    struct xml_geometry *geom;
    struct xml_tables_list *catalog = alloc_xml_tables ();

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
		  case ARG_CACHE_SIZE:
		      cache_size = atoi (argv[i]);
		      break;
		  case ARG_NAME_LEVEL:
		      name_level = atoi (argv[i]);
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
	  if (strcasecmp (argv[i], "--cache-size") == 0
	      || strcmp (argv[i], "-cs") == 0)
	    {
		next_arg = ARG_CACHE_SIZE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--name-level") == 0
	      || strcmp (argv[i], "-nl") == 0)
	    {
		next_arg = ARG_NAME_LEVEL;
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
	  if (strcasecmp (argv[i], "-dd") == 0)
	    {
		delete_duplicates = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--delete-duplicates") == 0)
	    {
		delete_duplicates = 1;
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
    if (delete_duplicates && name_level < 0)
      {
	  fprintf (stderr, "--delete-duplicates requires --name-level\n");
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

/* identifying the tables to be (possibly) collapsed */
    sql =
	"SELECT table_name, parent_table_name, xml_tag_namespace, xml_tag_name, "
	"gml_geometry_column, tree_level FROM xml_metacatalog_tables "
	"WHERE status LIKE 'raw %'";
    ret = sqlite3_get_table (handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT FROM xml_metacatalog_tables error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *table = results[(i * columns) + 0];
		const char *parent = results[(i * columns) + 1];
		const char *tag_ns = results[(i * columns) + 2];
		const char *tag_name = results[(i * columns) + 3];
		const char *geometry = results[(i * columns) + 4];
		int level = atoi (results[(i * columns) + 5]);
		add_xml_table (catalog, table, parent, tag_ns, tag_name,
			       geometry, level);
	    }
      }
    sqlite3_free_table (results);

    identify_childs (catalog);
    while (loop_again)
      {
	  /* iteratively attempting to collapse */
	  struct xml_table *tbl = catalog->first;
	  loop_again = 0;
	  while (tbl != NULL)
	    {
		if (tbl->first == NULL && tbl->done == 0)
		  {
		      char *parent =
			  check_xml_child_table (handle, tbl->table_name);
		      loop_again = 1;
		      tbl->done = 1;
		      if (parent != NULL)
			{
			    if (check_collapsible
				(handle, tbl->table_name, parent))
			      {
				  struct new_attributes *list =
				      upgrade_parent (handle, tbl->table_name,
						      parent, tbl->tag_name,
						      tbl->geometry);
				  if (list != NULL)
				    {
					get_xml_references (handle, list,
							    tbl->table_name);
					ret =
					    collapse_table (handle, journal_off,
							    list,
							    tbl->table_name,
							    parent, tbl);
					destroy_new_attributes (list);
					if (!ret)
					  {
					      fprintf (stderr,
						       "Unable to collapse \"%s\"\n",
						       tbl->table_name);
					      free (parent);
					      goto abort;
					  }
				    }
			      }
			    free (parent);
			}
		  }
		tbl = tbl->next;
	    }
      }

    if (delete_duplicates)
      {
	  /* deleting duplicates */
	  struct xml_table *tbl = catalog->first;
	  while (tbl != NULL)
	    {
		if (tbl->level == name_level)
		    remove_duplicates (handle, tbl);
		tbl = tbl->next;
	    }
      }

/* identifying the Geometries to be (possibly) recovered */
    sql =
	"SELECT table_name, column_name FROM xml_metacatalog_columns "
	"WHERE origin LIKE 'GeomFromGml%'";
    ret = sqlite3_get_table (handle, sql, &results, &rows, &columns, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "SELECT FROM xml_metacatalog_columns error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *table = results[(i * columns) + 0];
		const char *geometry = results[(i * columns) + 1];
		add_xml_geometry (catalog, table, geometry);
	    }
      }
    sqlite3_free_table (results);

    geom = catalog->first_geom;
    while (geom != NULL)
      {
	  check_geometry (handle, geom);
	  if (geom->type != NULL)
	      recover_geometry (handle, geom);
	  geom = geom->next;
      }

  abort:
    destroy_xml_tables (catalog);

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
    return 0;
}
