/* 
/ spatialite_xml_load
/
/ a tool loading any XML into SQLite tables
/
/ version 1.0, 2013 August 14
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

#include <sys/time.h>

#if defined(_WIN32) && !defined(__MINGW32__)
/* MSVC strictly requires this include [off_t] */
#include <sys/types.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include <expat.h>

#include "config.h"

#ifdef SPATIALITE_AMALGAMATION
#include <spatialite/sqlite3.h>
#else
#include <sqlite3.h>
#endif
#include <spatialite.h>

#define BUFFSIZE	8192

#define ARG_NONE	0
#define ARG_XML_PATH	1
#define ARG_DB_PATH	2
#define ARG_NAME_LEVEL 3
#define ARG_PARENT_LEVELS	4
#define ARG_CACHE_SIZE 5

struct gmlDynBuffer
{
/* a struct handling a dynamically growing output buffer */
    char *Buffer;
    size_t WriteOffset;
    size_t BufferSize;
    int Error;
};

struct sql_table
{
/* a SQL table name */
    char *table_name;
    char *parent_table;
    struct xml_tag *tag;
    sqlite3_stmt *ins_stmt;
    sqlite3_stmt *upd_stmt;
    sqlite3_int64 current;
    struct sql_table *next;
};

struct xml_attr
{
/* an XML attribute <tag attr="value"> */
    char *attr_ns;
    char *attr_name;
    char *attr_value;
    int exists;
    struct xml_attr *next;
};

struct xml_tag
{
/* an XML node <tag name> */
    char *tag_ns;
    char *tag_name;
    char *full_name;
    char *unique_name;
    int unique_level;
    int has_geometry;
    int tree_level;
    struct xml_attr *first_attr;
    struct xml_attr *last_attr;
    struct xml_tag *parent;
    struct sql_table *table;
    struct xml_tag *next;
};

struct stack_entry
{
/* an entry into the tag stack */
    char *tag_ns;
    char *tag_name;
    struct stack_entry *prev;
};

struct xml_params
{
/* an auxiliary struct used for GML parsing */
    char *filename;
    sqlite3 *db_handle;
    int journal_off;
    int collapsed_gml;
    int xlink_href;
    char *CharData;
    int CharDataLen;
    int CharDataMax;
    int CharDataStep;
    int parse_error;
    int db_error;
    struct xml_tag *first_tag;
    struct xml_tag *last_tag;
    struct xml_tag **sort_array;
    int count_array;
    struct sql_table *first_table;
    struct sql_table *last_table;
    struct stack_entry *stack;
    int CollapsingGML;
    struct gmlDynBuffer *CollapsedGML;
    char *CollapsedGMLMarker;
    int treeLevel;
};

static struct gmlDynBuffer *
gmlDynBufferAlloc (void)
{
/* creating and initializing a dynamically growing output buffer */
    struct gmlDynBuffer *buf = malloc (sizeof (struct gmlDynBuffer));
    buf->Buffer = NULL;
    buf->WriteOffset = 0;
    buf->BufferSize = 0;
    buf->Error = 0;
    return buf;
}

static void
gmlDynBufferDestroy (struct gmlDynBuffer *buf)
{
/* cleaning a dynamically growing output buffer */
    if (buf == NULL)
	return;
    if (buf->Buffer)
	free (buf->Buffer);
    free (buf);
}

static void
gmlDynBufferAppend (struct gmlDynBuffer *buf, const char *payload, size_t size)
{
/* appending into the buffer */
    size_t free_size = buf->BufferSize - buf->WriteOffset;
    if (size > free_size)
      {
	  /* we must allocate a bigger buffer */
	  size_t new_size;
	  char *new_buf;
	  if (buf->BufferSize == 0)
	      new_size = size + 1024;
	  else if (buf->BufferSize <= 4196)
	      new_size = buf->BufferSize + size + 4196;
	  else if (buf->BufferSize <= 65536)
	      new_size = buf->BufferSize + size + 65536;
	  else
	      new_size = buf->BufferSize + size + (1024 * 1024);
	  new_buf = malloc (new_size);
	  if (!new_buf)
	    {
		buf->Error = 1;
		return;
	    }
	  if (buf->Buffer)
	    {
		memcpy (new_buf, buf->Buffer, buf->WriteOffset);
		free (buf->Buffer);
	    }
	  buf->Buffer = new_buf;
	  buf->BufferSize = new_size;
      }
    memcpy (buf->Buffer + buf->WriteOffset, payload, size);
    buf->WriteOffset += size;
}

static void
xmlCharData (void *data, const XML_Char * s, int len)
{
/* parsing XML char data */
    struct xml_params *params = (struct xml_params *) data;
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

static int
valid_char_data (struct xml_params *params)
{
/* testing if Char Data are meaningfull */
    int valid = 0;
    if (params->CharDataLen > 0)
      {
	  int i;
	  for (i = 0; i < params->CharDataLen; i++)
	    {
		if (*(params->CharData + i) == ' ')
		    continue;
		if (*(params->CharData + i) == '\t')
		    continue;
		if (*(params->CharData + i) == '\r')
		    continue;
		if (*(params->CharData + i) == '\n')
		    continue;
		valid = 1;
		break;
	    }
      }
    return valid;
}

static int
tag_compare (const void *p1, const void *p2)
{
/* comparison function for QSort and BSearch*/
    int ret;
    struct xml_tag *tag1 = *((struct xml_tag **) p1);
    struct xml_tag *tag2 = *((struct xml_tag **) p2);
    ret = strcmp (tag1->full_name, tag2->full_name);
    if (ret != 0)
	return ret;
    if (tag1->tag_ns == NULL && tag2->tag_ns != NULL)
	return -1;
    if (tag1->tag_ns != NULL && tag2->tag_ns == NULL)
	return 1;
    if (tag1->tag_ns != NULL && tag2->tag_ns != NULL)
      {
	  ret = strcmp (tag1->tag_ns, tag2->tag_ns);
	  if (ret != 0)
	      return ret;
      }
    return strcmp (tag1->tag_name, tag2->tag_name);
}

static void
sort_tag_array (struct xml_params *params)
{
/* updating the Sorted Tags Array */
    int count = 0;
    struct xml_tag *tag = params->first_tag;
    while (tag != NULL)
      {
	  /* counting how many tags are there */
	  count++;
	  tag = tag->next;
      }
    if (params->sort_array != NULL)
	free (params->sort_array);
/* allocating the array */
    params->count_array = count;
    params->sort_array = malloc (sizeof (struct xml_tag **) * count);
    count = 0;
    tag = params->first_tag;
    while (tag != NULL)
      {
	  /* inserting the pointers into the array */
	  *(params->sort_array + count++) = tag;
	  tag = tag->next;
      }
    qsort (params->sort_array, params->count_array, sizeof (struct xml_tag *),
	   tag_compare);
}

static struct xml_attr *
alloc_attr (char *ns, char *name)
{
/* allocating and initializing an attribute */
    struct xml_attr *attr = malloc (sizeof (struct xml_attr));
    attr->attr_ns = ns;
    attr->attr_name = name;
    attr->attr_value = NULL;
    attr->exists = 0;
    attr->next = NULL;
    return attr;
}

static void
destroy_attr (struct xml_attr *attr)
{
/* memory cleanup - freeing an attribute */
    if (attr == NULL)
	return;
    if (attr->attr_ns != NULL)
	free (attr->attr_ns);
    if (attr->attr_name != NULL)
	free (attr->attr_name);
    if (attr->attr_value != NULL)
	free (attr->attr_value);
    free (attr);
}

static struct sql_table *
alloc_table (char *table_name, char *parent_table)
{
/* allocating and initializing a SQL Table */
    int len;
    int i;
    struct sql_table *table = malloc (sizeof (struct sql_table));
    table->table_name = table_name;
    if (table_name != NULL)
      {
	  len = strlen (table->table_name);
	  for (i = 0; i < len; i++)
	    {
		char c = *(table->table_name + i);
		if (c >= 'A' && c <= 'Z')
		  {
		      /* forcing to lowercase */
		      c = c - 'A' + 'a';
		      *(table->table_name + i) = c;
		  }
	    }
      }
    table->parent_table = parent_table;
    if (parent_table != NULL)
      {
	  len = strlen (table->parent_table);
	  for (i = 0; i < len; i++)
	    {
		char c = *(table->parent_table + i);
		if (c >= 'A' && c <= 'Z')
		  {
		      /* forcing to lowercase */
		      c = c - 'A' + 'a';
		      *(table->parent_table + i) = c;
		  }
	    }
      }
    table->tag = NULL;
    table->ins_stmt = NULL;
    table->upd_stmt = NULL;
    table->current = 0;
    table->next = NULL;
    return table;
}

static void
destroy_table (struct sql_table *table)
{
/* memory cleanup - freeing a SQL Table */
    if (table == NULL)
	return;
    if (table->ins_stmt != NULL)
	sqlite3_finalize (table->ins_stmt);
    if (table->upd_stmt != NULL)
	sqlite3_finalize (table->upd_stmt);
    free (table);
}

static struct xml_tag *
alloc_tag (const char *tag_ns, const char *tag_name, char *full_name,
	   struct xml_tag *parent)
{
/* allocating and initializing a tag */
    int len;
    struct xml_tag *tag = malloc (sizeof (struct xml_tag));
    if (tag_ns == NULL)
	tag->tag_ns = NULL;
    else
      {
	  len = strlen (tag_ns);
	  tag->tag_ns = malloc (len + 1);
	  strcpy (tag->tag_ns, tag_ns);
      }
    len = strlen (tag_name);
    tag->tag_name = malloc (len + 1);
    strcpy (tag->tag_name, tag_name);
    tag->full_name = full_name;
    tag->unique_name = NULL;
    tag->first_attr = NULL;
    tag->last_attr = NULL;
    tag->parent = parent;
    tag->unique_level = -1;
    tag->table = NULL;
    tag->has_geometry = 0;
    tag->tree_level = -1;
    tag->next = NULL;
    return tag;
}

static void
destroy_tag (struct xml_tag *tag)
{
/* memory cleanup - freeing a tag */
    struct xml_attr *pa;
    struct xml_attr *pan;
    if (tag == NULL)
	return;
    if (tag->tag_ns != NULL)
	free (tag->tag_ns);
    if (tag->tag_name != NULL)
	free (tag->tag_name);
    if (tag->full_name != NULL)
	sqlite3_free (tag->full_name);
    if (tag->unique_name != NULL)
	sqlite3_free (tag->unique_name);
    pa = tag->first_attr;
    while (pa != NULL)
      {
	  pan = pa->next;
	  destroy_attr (pa);
	  pa = pan;
      }
    free (tag);
}

static void
reset_tag_attributes (struct xml_tag *tag)
{
/* resetting XML attributes to NULL */
    struct xml_attr *attr = tag->first_attr;
    while (attr != NULL)
      {
	  if (attr->attr_value != NULL)
	      free (attr->attr_value);
	  attr->attr_value = NULL;
	  attr = attr->next;
      }
}

static struct xml_tag *
find_tag (struct xml_params *params, const char *tag_ns, const char *tag_name,
	  const char *full_name)
{
/* attempts to find if some XML Node tag is already defined */
    struct xml_tag item;
    struct xml_tag *p_item = &item;
    struct xml_tag *found;
    void *x;
    if (params->sort_array == NULL)
	return NULL;
    item.tag_ns = (char *) tag_ns;
    item.tag_name = (char *) tag_name;
    item.full_name = (char *) full_name;
    x = bsearch (&p_item, params->sort_array, params->count_array,
		 sizeof (struct xml_tag *), tag_compare);
    if (x == NULL)
	return NULL;
    found = *((struct xml_tag **) x);
    return found;
}

static struct xml_tag *
append_tag (struct xml_params *params, const char *tag_ns,
	    const char *tag_name, char *full_name, struct xml_tag *parent)
{
/* appending an XML Node tag */
    struct xml_tag *tag = alloc_tag (tag_ns, tag_name, full_name, parent);
    tag->tree_level = params->treeLevel;
    if (params->first_tag == NULL)
	params->first_tag = tag;
    if (params->last_tag != NULL)
	params->last_tag->next = tag;
    params->last_tag = tag;
    sort_tag_array (params);
    return tag;
}

static struct xml_attr *
find_attr (struct xml_tag *tag, const char *ns, const char *name)
{
/* attempts to find if some Attribute is already defined */
    struct xml_attr *p = tag->first_attr;
    while (p != NULL)
      {
	  if (p->attr_ns == NULL && ns == NULL)
	    {
		if (strcmp (p->attr_name, name) == 0)
		    return p;
	    }
	  if (p->attr_ns != NULL && ns != NULL)
	    {
		if (strcmp (p->attr_ns, ns) == 0
		    && strcmp (p->attr_name, name) == 0)
		    return p;
	    }
	  p = p->next;
      }
    return NULL;
}

static void
append_attribute (struct xml_tag *tag, char *ns, char *name)
{
/* appending an attribute to an XML Node */
    struct xml_attr *attr = find_attr (tag, ns, name);
    if (attr != NULL)
      {
	  /* already existing */
	  if (ns != NULL)
	      free (ns);
	  if (name != NULL)
	      free (name);
	  return;
      }
    attr = alloc_attr (ns, name);
    if (tag->first_attr == NULL)
	tag->first_attr = attr;
    if (tag->last_attr != NULL)
	tag->last_attr->next = attr;
    tag->last_attr = attr;
}

static void
set_attribute_value (struct xml_tag *tag, const char *ns, const char *name,
		     const char *value)
{
/* setting some value to an Attribute */
    int len;
    struct xml_attr *attr = find_attr (tag, ns, name);
    if (attr == NULL)
	return;
    len = strlen (value);
    if (attr->attr_value != NULL)
	free (attr->attr_value);
    attr->attr_value = malloc (len + 1);
    strcpy (attr->attr_value, value);
}

static void
push_stack (struct xml_params *params, char *tag_ns, char *tag_name)
{
/* pushing an item into the tag stack */
    struct stack_entry *entry = malloc (sizeof (struct stack_entry));
    entry->tag_ns = tag_ns;
    entry->tag_name = tag_name;
    entry->prev = params->stack;
    params->stack = entry;
}

static void
pop_stack (struct xml_params *params)
{
/* popping an item from the tag stack */
    struct stack_entry *entry = params->stack;
    if (entry != NULL)
      {
	  params->stack = entry->prev;
	  if (entry->tag_ns != NULL)
	      free (entry->tag_ns);
	  if (entry->tag_name != NULL)
	      free (entry->tag_name);
	  free (entry);
      }
}

static void
append_table (struct xml_params *ptr, struct xml_tag *xml, char *table_name,
	      char *parent_table)
{
/* appends a SQL Table definition */
    struct sql_table *tbl = alloc_table (table_name, parent_table);
    xml->table = tbl;
    tbl->tag = xml;
    if (ptr->first_table == NULL)
	ptr->first_table = tbl;
    if (ptr->last_table != NULL)
	ptr->last_table->next = tbl;
    ptr->last_table = tbl;
}

static char *
build_unique_name (struct xml_tag *tag, int name_level, int parent_levels)
{
/* building a qualified tag name */
    int i;
    char *str = NULL;
    struct xml_tag *parent = tag->parent;
    if (tag->tree_level <= name_level)
      {
	  /* above the limit: main branches of the tree -> "child_parent" */
	  if (parent == NULL)
	      str = sqlite3_mprintf ("%s", tag->tag_name);
	  else
	      str = sqlite3_mprintf ("%s_%s", parent->tag_name, tag->tag_name);
      }
    else
      {
	  /* under the limit: expanding a full tree branch */
	  int cnt = 0;
	  int max = 999999;
	  if (parent_levels >= 0)
	      max = parent_levels;
	  str = sqlite3_mprintf ("%s", tag->tag_name);
	  for (i = tag->tree_level - 1; i >= name_level; i--)
	    {
		char *tmp;
		if (parent == NULL)
		  {
		      sqlite3_free (str);
		      return NULL;
		  }
		if (cnt < max || i == name_level)
		  {
		      tmp = sqlite3_mprintf ("%s_%s", parent->tag_name, str);
		      sqlite3_free (str);
		      str = tmp;
		  }
		cnt++;
		parent = parent->parent;
	    }
      }
    for (i = 0; i < (int) strlen (str); i++)
      {
	  char c = *(str + i);
	  if (c >= 'A' && c <= 'Z')
	      *(str + i) = c - 'A' + 'a';
      }
    return str;
}

static void
set_unique_names (struct xml_params *ptr, int name_level, int parent_levels)
{
/* setting the appropriate Unique Names */
    struct xml_tag *pt = ptr->first_tag;
    while (pt != NULL)
      {
	  char *candidate = build_unique_name (pt, name_level, parent_levels);
	  if (pt->unique_name != NULL)
	      sqlite3_free (pt->unique_name);
	  pt->unique_name = candidate;
	  pt = pt->next;
      }
}

static void
set_table_names (struct xml_params *ptr)
{
/* creating the SQL table names */
    struct xml_tag *pt = ptr->first_tag;
    while (pt != NULL)
      {
	  struct xml_tag *parent = pt->parent;
	  char *table_name = pt->unique_name;
	  char *parent_table = NULL;
	  if (parent != NULL)
	      parent_table = parent->unique_name;
	  append_table (ptr, pt, table_name, parent_table);
	  pt = pt->next;
      }
}

static void
set_xml_filename (struct xml_params *ptr, const char *path)
{
/* saving the XML (input) filename */
    int len = strlen (path);
    char *xpath = malloc (len + 1);
    const char *filename;
    strcpy (xpath, path);
    filename = basename (xpath);
    len = strlen (filename);
    ptr->filename = malloc (len + 1);
    strcpy (ptr->filename, filename);
    free (xpath);
}

static void
params_cleanup (struct xml_params *ptr)
{
/* memory cleanup - params */
    struct xml_tag *pt;
    struct xml_tag *ptn;
    struct sql_table *ps;
    struct sql_table *psn;
    if (ptr == NULL)
	return;

    pt = ptr->first_tag;
    while (pt != NULL)
      {
	  ptn = pt->next;
	  destroy_tag (pt);
	  pt = ptn;
      }
    while (1)
      {
	  if (ptr->stack == NULL)
	      break;
	  pop_stack (ptr);
      }
    ps = ptr->first_table;
    while (ps != NULL)
      {
	  psn = ps->next;
	  destroy_table (ps);
	  ps = psn;
      }
    if (ptr->filename != NULL)
	free (ptr->filename);
    if (ptr->CharData != NULL)
	free (ptr->CharData);
    if (ptr->sort_array != NULL)
	free (ptr->sort_array);
    if (ptr->CollapsedGML != NULL)
	gmlDynBufferDestroy (ptr->CollapsedGML);
    if (ptr->CollapsedGMLMarker != NULL)
	sqlite3_free (ptr->CollapsedGMLMarker);
}

static void
split_namespace (const char *str, char **ns, char **name)
{
/* attempting to divide an XML identifier into a namespace and a name */
    int i;
    int len;
    int pos = -1;
    *ns = NULL;
    *name = NULL;

    len = strlen (str);
    for (i = 0; i < len; i++)
      {
	  if (str[i] == ':')
	    {
		pos = i;
		break;
	    }
      }
    if (pos < 0)
      {
	  *name = malloc (len + 1);
	  strcpy (*name, str);
	  return;
      }
    *ns = malloc (pos + 1);
    memset (*ns, '\0', pos + 1);
    memcpy (*ns, str, pos);
    len = strlen (str + pos + 1);
    *name = malloc (len + 1);
    strcpy (*name, str + pos + 1);
}

static int
is_gml_geometry (const char *ns, const char *name)
{
/* testing a possible GML Geometry */
    int is_gml = 0;
    int is_gml_ns = 0;
    if (strcmp (name, "Point") == 0)
	is_gml = 1;
    if (strcmp (name, "LineString") == 0)
	is_gml = 1;
    if (strcmp (name, "Curve") == 0)
	is_gml = 1;
    if (strcmp (name, "Polygon") == 0)
	is_gml = 1;
    if (strcmp (name, "MultiPoint") == 0)
	is_gml = 1;
    if (strcmp (name, "MultiLineString") == 0)
	is_gml = 1;
    if (strcmp (name, "MultiCurve") == 0)
	is_gml = 1;
    if (strcmp (name, "MultiPolygon") == 0)
	is_gml = 1;
    if (strcmp (name, "MultiSurface") == 0)
	is_gml = 1;
    if (strcmp (name, "MultiGeometry") == 0)
	is_gml = 1;
    if (ns == NULL)
	is_gml_ns = 1;
    else if (strcmp (ns, "gml") == 0)
	is_gml_ns = 1;
    if (!is_gml_ns)
	is_gml = 0;
    return is_gml;
}

static char *
build_full_name (struct stack_entry *entry, const char *name)
{
/* building a fully qualified tag name */
    int i;
    char *str = NULL;
    struct stack_entry *pe = entry;
    while (pe != NULL)
      {
	  if (str != NULL)
	    {
		char *tmp = sqlite3_mprintf ("%s_%s", pe->tag_name, str);
		sqlite3_free (str);
		str = tmp;
	    }
	  else
	      str = sqlite3_mprintf ("%s", pe->tag_name);
	  pe = pe->prev;
      }
    if (str != NULL)
      {
	  char *tmp = sqlite3_mprintf ("%s_%s", str, name);
	  sqlite3_free (str);
	  str = tmp;
      }
    else
	str = sqlite3_mprintf ("%s", name);
    for (i = 0; i < (int) strlen (str); i++)
      {
	  char c = *(str + i);
	  if (c >= 'A' && c <= 'Z')
	      *(str + i) = c - 'A' + 'a';
      }
    return str;
}

static void
start_tag_1 (void *data, const char *el, const char **attr)
{
/* XML element starting - Pass I */
    const char **attrib = attr;
    int count = 0;
    const char *k;
    char *ns = NULL;
    char *name = NULL;
    char *full_name = NULL;
    char *parent_full_name = NULL;
    struct xml_params *params = (struct xml_params *) data;
    struct xml_tag *tag;
    if (params->CollapsingGML)
      {
	  /* collapsing GML Geometries */
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    split_namespace (el, &ns, &name);
    if (params->collapsed_gml)
      {
	  /* attempting to collapse GML Geometries */
	  if (is_gml_geometry (ns, name))
	    {
		if (ns != NULL)
		    free (ns);
		if (name != NULL)
		    free (name);
		params->CollapsingGML = 1;
		if (params->CollapsedGMLMarker != NULL)
		    sqlite3_free (params->CollapsedGMLMarker);
		params->CollapsedGMLMarker = sqlite3_mprintf ("%s", el);
		*(params->CharData) = '\0';
		params->CharDataLen = 0;
		return;
	    }
      }
    params->treeLevel += 1;
    full_name = build_full_name (params->stack, name);
    tag = find_tag (params, ns, name, full_name);
    if (tag == NULL)
      {
	  struct xml_tag *parent = NULL;
	  if (params->stack != NULL)
	    {
		struct stack_entry *entry = params->stack;
		parent_full_name =
		    build_full_name (entry->prev, entry->tag_name);
		parent =
		    find_tag (params, entry->tag_ns, entry->tag_name,
			      parent_full_name);
		sqlite3_free (parent_full_name);
	    }
	  tag = append_tag (params, ns, name, full_name, parent);
      }
    else
	sqlite3_free (full_name);
    push_stack (params, ns, name);
    while (*attrib != NULL)
      {
	  if ((count % 2) == 0)
	      k = *attrib;
	  else
	    {
		split_namespace (k, &ns, &name);
		append_attribute (tag, ns, name);
	    }
	  attrib++;
	  count++;
      }
    *(params->CharData) = '\0';
    params->CharDataLen = 0;
}

static void
end_tag_1 (void *data, const char *el)
{
/* XML element ending - Pass I */
    struct xml_params *params = (struct xml_params *) data;
    if (params->CollapsingGML)
      {
	  /* collapsing GML Geometries */
	  if (strcmp (params->CollapsedGMLMarker, el) == 0)
	    {
		char *full_name = build_full_name (params->stack->prev,
						   params->stack->tag_name);
		struct xml_tag *tag = find_tag (params, params->stack->tag_ns,
						params->stack->tag_name,
						full_name);
		sqlite3_free (full_name);
		tag->has_geometry = 1;
		params->CollapsingGML = 0;
	    }
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    params->treeLevel -= 1;
    *(params->CharData) = '\0';
    params->CharDataLen = 0;
    pop_stack (params);
}

static int
create_sql_table (sqlite3 * db_handle, struct sql_table *tbl, int xlink_href)
{
/* attempting to create a SQL Table (if not already existing) */
    int ret;
    char *err_msg = NULL;
    char *sql;
    struct xml_attr *attr;
    int xlink_href_index = 0;
    int gml_id_index = 0;
    sql = sqlite3_mprintf ("CREATE TABLE IF NOT EXISTS \"%s\" (\n"
			   "node_id INTEGER PRIMARY KEY AUTOINCREMENT,\n",
			   tbl->table_name);
    if (tbl->parent_table != NULL)
      {
	  /* defining the Foreign Key column */
	  char *sql1 = sqlite3_mprintf ("%sparent_id INTEGER NOT NULL,\n", sql);
	  sqlite3_free (sql);
	  sql = sql1;
      }
    else
      {
	  /* defining the "mtdapp_filename" column */
	  char *sql1 =
	      sqlite3_mprintf ("%smtdapp_filename TEXT NOT NULL,\n", sql);
	  sqlite3_free (sql);
	  sql = sql1;
      }
    attr = tbl->tag->first_attr;
    while (attr != NULL)
      {
	  /* defining any attribute  column */
	  char *sql1;
	  if (xlink_href)
	    {
		int is_xlink = 0;
		int is_gml = 0;
		if (attr->attr_ns != NULL)
		  {
		      if (strcmp (attr->attr_ns, "xlink") == 0)
			  is_xlink = 1;
		      if (strcmp (attr->attr_ns, "gml") == 0)
			  is_gml = 1;
		  }
		if (is_xlink && strcmp (attr->attr_name, "href") == 0)
		  {
		      sql1 =
			  sqlite3_mprintf ("%s\"%s\" TEXT,\n", sql,
					   "xlink_href");
		      sqlite3_free (sql);
		      sql = sql1;
		      xlink_href_index = 1;
		      attr = attr->next;
		      continue;
		  }
		if (is_gml && strcmp (attr->attr_name, "id") == 0)
		  {
		      sql1 =
			  sqlite3_mprintf ("%s\"%s\" TEXT,\n", sql, "gml_id");
		      sqlite3_free (sql);
		      sql = sql1;
		      gml_id_index = 1;
		      attr = attr->next;
		      continue;
		  }
	    }
	  sql1 = sqlite3_mprintf ("%s\"%s\" TEXT,\n", sql, attr->attr_name);
	  sqlite3_free (sql);
	  sql = sql1;
	  attr = attr->next;
      }
    if (tbl->parent_table != NULL)
      {
	  /* declaring the Foreign Key Constraint */
	  char *sql1;
	  if (tbl->tag->has_geometry)
	    {
		sql1 =
		    sqlite3_mprintf
		    ("%snode_value TEXT,\nfrom_gml_geometry BLOB\n,"
		     "CONSTRAINT \"fk_%s\" FOREIGN KEY (parent_id) "
		     "REFERENCES \"%s\" (node_id) ON DELETE CASCADE)", sql,
		     tbl->table_name, tbl->parent_table);
	    }
	  else
	    {
		sql1 = sqlite3_mprintf ("%snode_value TEXT,\n"
					"CONSTRAINT \"fk_%s\" FOREIGN KEY (parent_id) "
					"REFERENCES \"%s\" (node_id) ON DELETE CASCADE)",
					sql, tbl->table_name,
					tbl->parent_table);
	    }
	  sqlite3_free (sql);
	  sql = sql1;
      }
    else
      {
	  /* root table */
	  char *sql1;
	  if (tbl->tag->has_geometry)
	      sql1 =
		  sqlite3_mprintf
		  ("%snode_value TEXT,\nfrom_gml_geometry BLOB)", sql);
	  else
	      sql1 = sqlite3_mprintf ("%snode_value TEXT)", sql);
	  sqlite3_free (sql);
	  sql = sql1;
      }
/* executing the SQL statement */
    ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

    if (tbl->parent_table != NULL)
      {
	  /* creating an Index supporting the Foreign Key */
	  sql = sqlite3_mprintf ("CREATE INDEX IF NOT EXISTS "
				 "\"idx_%s\" ON \"%s\" (parent_id)",
				 tbl->table_name, tbl->table_name);
	  ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "CREATE INDEX [FK] error: %s\n", err_msg);
		sqlite3_free (err_msg);
		return 0;
	    }
      }

    if (xlink_href_index)
      {
	  /* creating an Index supporting the xlink_href column */
	  sql = sqlite3_mprintf ("CREATE INDEX IF NOT EXISTS "
				 "\"idx_%s_xlink_href\" ON \"%s\" (xlink_href)",
				 tbl->table_name, tbl->table_name);
	  ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "CREATE INDEX [xlink_href] error: %s\n",
			 err_msg);
		sqlite3_free (err_msg);
		return 0;
	    }
      }

    if (gml_id_index)
      {
	  /* creating an Index supporting the gnk_id column */
	  sql = sqlite3_mprintf ("CREATE INDEX IF NOT EXISTS "
				 "\"idx_%s_gml_id\" ON \"%s\" (gml_id)",
				 tbl->table_name, tbl->table_name);
	  ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "CREATE INDEX [gml_id] error: %s\n", err_msg);
		sqlite3_free (err_msg);
		return 0;
	    }
      }
    return 1;
}

static int
check_baseline_table (sqlite3 * db_handle, struct sql_table *tbl)
{
/* checking the expected baseline definition for some table */
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

/* checking the expected columns */
    char *sql = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", tbl->table_name);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA table_info error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
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
		      else
			{
			    fprintf (stderr,
				     "TABLE \"%s\": column \"node_id\" isn't defined as a PRIMARY KEY\n",
				     tbl->table_name);
			    error = 1;
			}
		  }
		else if (atoi (results[(i * columns) + 5]) == 1)
		  {
		      fprintf (stderr,
			       "TABLE \"%s\": unexpected column \"%s\" defined as a PRIMARY KEY\n",
			       tbl->table_name, name);
		      error = 1;
		  }
		if (strcmp (name, "parent_id") == 0)
		    ok_fk = 1;
		if (strcmp (name, "node_value") == 0)
		    ok_value = 1;
	    }
      }
    sqlite3_free_table (results);
    if (error)
	return 0;
    if (ok_pk == 0)
      {
	  fprintf (stderr,
		   "TABLE \"%s\": unable to found the expected \"node_id\" PRIMARY KEY\n",
		   tbl->table_name);
	  return 0;
      }
    if (tbl->parent_table != NULL && ok_fk == 0)
      {
	  fprintf (stderr,
		   "TABLE \"%s\": unable to found the expected \"parent_id\" column\n",
		   tbl->table_name);
	  return 0;
      }
    if (tbl->parent_table == NULL && ok_fk != 0)
      {
	  fprintf (stderr,
		   "TABLE \"%s\": found an unexpected \"parent_id\" column\n",
		   tbl->table_name);
	  return 0;
      }
    if (ok_value == 0)
      {
	  fprintf (stderr,
		   "TABLE \"%s\": unable to found the expected \"node_value\" column\n",
		   tbl->table_name);
	  return 0;
      }

    ok_fk = 0;
    if (tbl->parent_table != NULL)
      {
	  /* checking the Foreing Key */
	  char *sql = sqlite3_mprintf ("PRAGMA foreign_key_list(\"%s\")",
				       tbl->table_name);
	  ret =
	      sqlite3_get_table (db_handle, sql, &results, &rows, &columns,
				 NULL);
	  sqlite3_free (sql);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "PRAGMA foreign_key_list error: %s\n",
			 err_msg);
		sqlite3_free (err_msg);
		return 0;
	    }
	  if (rows < 1)
	      ;
	  else
	    {
		for (i = 1; i <= rows; i++)
		  {
		      const char *name = results[(i * columns) + 2];
		      if (strcmp (name, tbl->parent_table) != 0)
			{
			    fprintf (stderr,
				     "TABLE \"%s\": mismatching FOREIGN KEY [references \"%s\"]\n",
				     tbl->table_name, name);
			    error = 1;
			}
		      else
			  ok_fk = 1;
		      if (strcmp (results[(i * columns) + 3], "parent_id") != 0
			  || strcmp (results[(i * columns) + 4],
				     "node_id") != 0)
			{
			    fprintf (stderr,
				     "TABLE \"%s\": mismatching FOREIGN KEY [not \"parent_id\" -> \"node_id\"]\n",
				     tbl->table_name);
			    error = 1;
			}
		  }
	    }
	  sqlite3_free_table (results);
	  if (error)
	      return 0;
	  if (ok_fk == 0)
	    {
		fprintf (stderr,
			 "TABLE \"%s\": unable to found the expected FOREIGN KEY\n",
			 tbl->table_name);
		return 0;
	    }
      }
    return 1;
}

static int
upgrade_sql_table (sqlite3 * db_handle, struct sql_table *tbl, int xlink_href)
{
/* attempting to upgrade some table */
    int ret;
    char *err_msg = NULL;
    char **results;
    int rows;
    int columns;
    int i;
    struct xml_attr *attr;

/* checking the attribute columns */
    char *sql = sqlite3_mprintf ("PRAGMA table_info(\"%s\")", tbl->table_name);
    ret = sqlite3_get_table (db_handle, sql, &results, &rows, &columns, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "PRAGMA table_info error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		const char *name = results[(i * columns) + 1];
		attr = tbl->tag->first_attr;
		while (attr != NULL)
		  {
		      if (xlink_href)
			{
			    /* special cases: xlink_href and gml_id */
			    int is_xlink = 0;
			    int is_gml = 0;
			    if (attr->attr_ns != NULL)
			      {
				  if (strcmp (attr->attr_ns, "xlink") == 0)
				      is_xlink = 1;
				  if (strcmp (attr->attr_ns, "gml") == 0)
				      is_gml = 1;
			      }
			    if (is_xlink
				&& strcmp (attr->attr_name, "href") == 0)
			      {
				  if (strcmp (name, "xlink_href") == 0)
				    {
					attr->exists = 1;
					attr = attr->next;
					continue;
				    }
			      }
			    if (is_gml && strcmp (attr->attr_name, "id") == 0)
			      {
				  if (strcmp (name, "gml_id") == 0)
				    {
					attr->exists = 1;
					attr = attr->next;
					continue;
				    }
			      }
			}
		      if (strcmp (attr->attr_name, name) == 0)
			  attr->exists = 1;
		      attr = attr->next;
		  }
	    }
      }
    sqlite3_free_table (results);

    attr = tbl->tag->first_attr;
    while (attr != NULL)
      {
	  if (attr->exists != 1)
	    {
		/* attempting to add an attribute column */
		sql =
		    sqlite3_mprintf
		    ("ALTER TABLE \"%s\" ADD COLUMN \"%s\" TEXT",
		     tbl->table_name, attr->attr_name);
		ret = sqlite3_exec (db_handle, sql, NULL, NULL, &err_msg);
		sqlite3_free (sql);
		if (ret != SQLITE_OK)
		  {
		      fprintf (stderr, "ALTER TABLE error: %s\n", err_msg);
		      sqlite3_free (err_msg);
		      return 0;
		  }
	    }
	  attr = attr->next;
      }
    return 1;
}

static int
prepare_sql_statements (sqlite3 * db_handle, struct sql_table *tbl,
			int xlink_href)
{
/* preparing the INSERT and UPDATE SQL statements */
    struct xml_attr *attr;
    char *sql;
    char *sql1;
    int ret;
    sqlite3_stmt *stmt;

/* preparing the INSERT statement */
    if (tbl->parent_table == NULL)
	sql =
	    sqlite3_mprintf ("INSERT INTO \"%s\" (node_id, mtdapp_filename",
			     tbl->table_name);
    else
	sql =
	    sqlite3_mprintf ("INSERT INTO \"%s\" (node_id, parent_id",
			     tbl->table_name);
    attr = tbl->tag->first_attr;
    while (attr != NULL)
      {
	  if (xlink_href)
	    {
		/* special cases: xlink_href and gml_id */
		int is_xlink = 0;
		int is_gml = 0;
		if (attr->attr_ns != NULL)
		  {
		      if (strcmp (attr->attr_ns, "xlink") == 0)
			  is_xlink = 1;
		      if (strcmp (attr->attr_ns, "gml") == 0)
			  is_gml = 1;
		  }
		if (is_xlink && strcmp (attr->attr_name, "href") == 0)
		  {
		      sql1 = sqlite3_mprintf ("%s, xlink_href", sql);
		      sqlite3_free (sql);
		      sql = sql1;
		      attr = attr->next;
		      continue;
		  }
		if (is_gml && strcmp (attr->attr_name, "id") == 0)
		  {
		      sql1 = sqlite3_mprintf ("%s, gml_id", sql);
		      sqlite3_free (sql);
		      sql = sql1;
		      attr = attr->next;
		      continue;
		  }
	    }
	  sql1 = sqlite3_mprintf ("%s, \"%s\"", sql, attr->attr_name);
	  sqlite3_free (sql);
	  sql = sql1;
	  attr = attr->next;
      }
    sql1 = sqlite3_mprintf ("%s) VALUES (NULL, ?", sql);
    sqlite3_free (sql);
    sql = sql1;
    attr = tbl->tag->first_attr;
    while (attr != NULL)
      {
	  sql1 = sqlite3_mprintf ("%s, ?", sql);
	  sqlite3_free (sql);
	  sql = sql1;
	  attr = attr->next;
      }
    sql1 = sqlite3_mprintf ("%s)", sql);
    sqlite3_free (sql);
    sql = sql1;
    ret = sqlite3_prepare_v2 (db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "INSERT INTO error: %s\n",
		   sqlite3_errmsg (db_handle));
	  return 0;
      }
    tbl->ins_stmt = stmt;

/* preparing the UPDATE statement */
    if (tbl->tag->has_geometry)
	sql = sqlite3_mprintf ("UPDATE \"%s\" SET node_value = ?, "
			       "from_gml_geometry = ? WHERE node_id = ?",
			       tbl->table_name);
    else
	sql =
	    sqlite3_mprintf
	    ("UPDATE \"%s\" SET node_value = ? WHERE node_id = ?",
	     tbl->table_name);
    ret = sqlite3_prepare_v2 (db_handle, sql, strlen (sql), &stmt, NULL);
    sqlite3_free (sql);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "UPDATE error: %s\n", sqlite3_errmsg (db_handle));
	  return 0;
      }
    tbl->upd_stmt = stmt;
    return 1;
}

static int
prepare_sql_tables (struct xml_params *ptr)
{
/* creating/updating the SQL tables */
    int ret;
    int error = 0;
    const char *sql;
    char *err_msg;
    sqlite3_stmt *stmt;
    struct sql_table *tbl = ptr->first_table;
    while (tbl != NULL)
      {
	  ret = create_sql_table (ptr->db_handle, tbl, ptr->xlink_href);
	  if (!ret)
	      return 0;
	  ret = check_baseline_table (ptr->db_handle, tbl);
	  if (!ret)
	      return 0;
	  ret = upgrade_sql_table (ptr->db_handle, tbl, ptr->xlink_href);
	  if (!ret)
	      return 0;
	  ret = prepare_sql_statements (ptr->db_handle, tbl, ptr->xlink_href);
	  if (!ret)
	      return 0;
	  tbl = tbl->next;
      }

/* creating the "xml_metacatalog_tables" table */
    sql = "CREATE TABLE IF NOT EXISTS xml_metacatalog_tables (\n"
	"table_name TEXT NOT NULL PRIMARY KEY,\n"
	"tree_level INTEGER NOT NULL,\n"
	"xml_tag_namespace TEXT,\n"
	"xml_tag_name TEXT NOT NULL,\n"
	"parent_table_name TEXT,\n"
	"gml_geometry_column TEXT,\n"
	"status TEXT NOT NULL,\n"
	"CONSTRAINT fk_xml_meta_tables FOREIGN KEY (parent_table_name) "
	"REFERENCES xml_metacatalog_tables (table_name))";
    ret = sqlite3_exec (ptr->db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE xml_meta_catalog_tables error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* prepared statement for INSERT INTO xml_metacatalog_tables */
    sql =
	"INSERT OR IGNORE INTO xml_metacatalog_tables (table_name, tree_level, "
	"xml_tag_namespace, xml_tag_name, parent_table_name, "
	"gml_geometry_column, status) VALUES (?, ?, ?, ?, ?, ?, ?)";
    ret = sqlite3_prepare_v2 (ptr->db_handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "INSERT INTO xml_metacatalog_tables error: %s\n",
		   sqlite3_errmsg (ptr->db_handle));
	  return 0;
      }

/* updating "xml_metacatalog_tables" */
    tbl = ptr->first_table;
    while (tbl != NULL)
      {
	  const char *str;
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, tbl->table_name, strlen (tbl->table_name),
			     SQLITE_STATIC);
	  sqlite3_bind_int (stmt, 2, tbl->tag->tree_level);
	  if (tbl->tag->tag_ns == NULL)
	      sqlite3_bind_null (stmt, 3);
	  else
	      sqlite3_bind_text (stmt, 3, tbl->tag->tag_ns,
				 strlen (tbl->tag->tag_ns), SQLITE_STATIC);
	  sqlite3_bind_text (stmt, 4, tbl->tag->tag_name,
			     strlen (tbl->tag->tag_name), SQLITE_STATIC);
	  if (tbl->parent_table == NULL)
	      sqlite3_bind_null (stmt, 5);
	  else
	      sqlite3_bind_text (stmt, 5, tbl->parent_table,
				 strlen (tbl->parent_table), SQLITE_STATIC);
	  if (tbl->tag->has_geometry)
	    {
		str = "from_gml_geometry";
		sqlite3_bind_text (stmt, 6, str, strlen (str), SQLITE_STATIC);
	    }
	  else
	      sqlite3_bind_null (stmt, 6);
	  str = "raw (not post-processed)";
	  sqlite3_bind_text (stmt, 7, str, strlen (str), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr, "INSERT xml_metacatalog_tables error: %s\n",
			 sqlite3_errmsg (ptr->db_handle));
		error = 1;
		goto stop;
	    }
	  tbl = tbl->next;
      }
  stop:
    sqlite3_finalize (stmt);

/* creating the "xml_metacatalog_columns" table */
    sql = "CREATE TABLE IF NOT EXISTS xml_metacatalog_columns (\n"
	"table_name TEXT NOT NULL,\n"
	"column_name TEXT NOT NULL,\n"
	"origin TEXT NOT NULL,\n"
	"destination TEXT,\n"
	"xml_reference TEXT NOT NULL,\n"
	"CONSTRAINT pk_xml_meta_columns PRIMARY KEY (table_name, column_name),\n"
	"CONSTRAINT fk_xml_meta_columns FOREIGN KEY (table_name) "
	"REFERENCES xml_metacatalog_tables (table_name))";
    ret = sqlite3_exec (ptr->db_handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "CREATE TABLE xml_meta_catalog_columns error: %s\n",
		   err_msg);
	  sqlite3_free (err_msg);
	  return 0;
      }

/* prepared statement for INSERT INTO xml_metacatalog_columns */
    sql =
	"INSERT OR IGNORE INTO xml_metacatalog_columns (table_name, column_name, "
	"origin, destination, xml_reference) VALUES (?, ?, ?, NULL, ?)";
    ret = sqlite3_prepare_v2 (ptr->db_handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "INSERT INTO xml_metacatalog_columns error: %s\n",
		   sqlite3_errmsg (ptr->db_handle));
	  return 0;
      }

/* updating "xml_metacatalog_columns" */
    tbl = ptr->first_table;
    while (tbl != NULL)
      {
	  char *xml;
	  const char *str;
	  struct xml_attr *attr;
	  /* node_id column */
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, tbl->table_name, strlen (tbl->table_name),
			     SQLITE_STATIC);
	  str = "node_id";
	  sqlite3_bind_text (stmt, 2, str, strlen (str), SQLITE_STATIC);
	  str = "DBMS internal identifier - Primary Key";
	  sqlite3_bind_text (stmt, 3, str, strlen (str), SQLITE_STATIC);
	  str = "n.a.";
	  sqlite3_bind_text (stmt, 4, str, strlen (str), SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr, "INSERT xml_metacatalog_columns error: %s\n",
			 sqlite3_errmsg (ptr->db_handle));
		error = 1;
		goto stop2;
	    }
	  if (tbl->parent_table != NULL)
	    {
		/* parent_id column */
		sqlite3_reset (stmt);
		sqlite3_clear_bindings (stmt);
		sqlite3_bind_text (stmt, 1, tbl->table_name,
				   strlen (tbl->table_name), SQLITE_STATIC);
		str = "parent_id";
		sqlite3_bind_text (stmt, 2, str, strlen (str), SQLITE_STATIC);
		str = "DBMS internal identifier - Foreign Key";
		sqlite3_bind_text (stmt, 3, str, strlen (str), SQLITE_STATIC);
		str = "n.a.";
		sqlite3_bind_text (stmt, 4, str, strlen (str), SQLITE_STATIC);
		ret = sqlite3_step (stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr,
			       "INSERT xml_metacatalog_columns error: %s\n",
			       sqlite3_errmsg (ptr->db_handle));
		      error = 1;
		      goto stop2;
		  }
	    }
	  /* node_value column */
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_text (stmt, 1, tbl->table_name, strlen (tbl->table_name),
			     SQLITE_STATIC);
	  str = "node_value";
	  sqlite3_bind_text (stmt, 2, str, strlen (str), SQLITE_STATIC);
	  str = "imported from XML Node value";
	  sqlite3_bind_text (stmt, 3, str, strlen (str), SQLITE_STATIC);
	  if (tbl->tag->tag_ns == NULL)
	      xml =
		  sqlite3_mprintf ("<%s>value</%s>",
				   tbl->tag->tag_name, tbl->tag->tag_name);
	  else
	      xml =
		  sqlite3_mprintf ("<%s:%s>value</%s:%s>",
				   tbl->tag->tag_ns,
				   tbl->tag->tag_name,
				   tbl->tag->tag_ns, tbl->tag->tag_name);
	  sqlite3_bind_text (stmt, 4, xml, strlen (xml), sqlite3_free);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		fprintf (stderr, "INSERT xml_metacatalog_columns error: %s\n",
			 sqlite3_errmsg (ptr->db_handle));
		error = 1;
		goto stop2;
	    }
	  if (tbl->tag->has_geometry == 0)
	    {
		/* node_value_column */
		sqlite3_reset (stmt);
		sqlite3_clear_bindings (stmt);
		sqlite3_bind_text (stmt, 1, tbl->table_name,
				   strlen (tbl->table_name), SQLITE_STATIC);
		str = "node_value";
		sqlite3_bind_text (stmt, 2, str, strlen (str), SQLITE_STATIC);
		str = "imported from XML node value";
		sqlite3_bind_text (stmt, 3, str, strlen (str), SQLITE_STATIC);
		if (tbl->tag->tag_ns == NULL)
		    xml =
			sqlite3_mprintf ("<%s>value</%s>", tbl->tag->tag_name,
					 tbl->tag->tag_name);
		else
		    xml =
			sqlite3_mprintf ("<%s:%s>value</%s:%s>",
					 tbl->tag->tag_ns, tbl->tag->tag_name,
					 tbl->tag->tag_ns, tbl->tag->tag_name);
		sqlite3_bind_text (stmt, 4, xml, strlen (xml), sqlite3_free);
		ret = sqlite3_step (stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr,
			       "INSERT xml_metacatalog_columns error: %s\n",
			       sqlite3_errmsg (ptr->db_handle));
		      error = 1;
		      goto stop2;
		  }
	    }
	  else
	    {
		/* collapsed GML Geometry */
		sqlite3_reset (stmt);
		sqlite3_clear_bindings (stmt);
		sqlite3_bind_text (stmt, 1, tbl->table_name,
				   strlen (tbl->table_name), SQLITE_STATIC);
		str = "node_value";
		sqlite3_bind_text (stmt, 2, str, strlen (str), SQLITE_STATIC);
		str = "imported from collapsed GML Geometry tags";
		sqlite3_bind_text (stmt, 3, str, strlen (str), SQLITE_STATIC);
		if (tbl->tag->tag_ns == NULL)
		    xml =
			sqlite3_mprintf ("<%s>gml_expr</%s>",
					 tbl->tag->tag_name,
					 tbl->tag->tag_name);
		else
		    xml =
			sqlite3_mprintf ("<%s:%s>gml_expr</%s:%s>",
					 tbl->tag->tag_ns, tbl->tag->tag_name,
					 tbl->tag->tag_ns, tbl->tag->tag_name);
		sqlite3_bind_text (stmt, 4, xml, strlen (xml), sqlite3_free);
		ret = sqlite3_step (stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr,
			       "INSERT xml_metacatalog_columns error: %s\n",
			       sqlite3_errmsg (ptr->db_handle));
		      error = 1;
		      goto stop2;
		  }
		sqlite3_reset (stmt);
		sqlite3_clear_bindings (stmt);
		sqlite3_bind_text (stmt, 1, tbl->table_name,
				   strlen (tbl->table_name), SQLITE_STATIC);
		str = "from_gml_geometry";
		sqlite3_bind_text (stmt, 2, str, strlen (str), SQLITE_STATIC);
		str = "GeomFromGML(node_value)";
		sqlite3_bind_text (stmt, 3, str, strlen (str), SQLITE_STATIC);
		if (tbl->tag->tag_ns == NULL)
		    xml =
			sqlite3_mprintf ("<%s>gml_expr</%s>",
					 tbl->tag->tag_name,
					 tbl->tag->tag_name);
		else
		    xml =
			sqlite3_mprintf ("<%s:%s>gml_expr</%s:%s>",
					 tbl->tag->tag_ns, tbl->tag->tag_name,
					 tbl->tag->tag_ns, tbl->tag->tag_name);
		sqlite3_bind_text (stmt, 4, xml, strlen (xml), sqlite3_free);
		ret = sqlite3_step (stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr,
			       "INSERT xml_metacatalog_columns error: %s\n",
			       sqlite3_errmsg (ptr->db_handle));
		      error = 1;
		      goto stop2;
		  }
	    }
	  attr = tbl->tag->first_attr;
	  while (attr != NULL)
	    {
		/* XML attributes */
		sqlite3_reset (stmt);
		sqlite3_clear_bindings (stmt);
		sqlite3_bind_text (stmt, 1, tbl->table_name,
				   strlen (tbl->table_name), SQLITE_STATIC);
		if (ptr->xlink_href)
		  {
		      int is_xlink = 0;
		      int is_gml = 0;
		      if (attr->attr_ns != NULL)
			{
			    if (strcmp (attr->attr_ns, "xlink") == 0)
				is_xlink = 1;
			    if (strcmp (attr->attr_ns, "gml") == 0)
				is_gml = 1;
			}
		      if (is_xlink && strcmp (attr->attr_name, "href") == 0)
			{
			    const char *xlhr = "xlink_href";
			    sqlite3_bind_text (stmt, 2, xlhr, strlen (xlhr),
					       SQLITE_STATIC);
			}
		      else if (is_gml && strcmp (attr->attr_name, "id") == 0)
			{
			    const char *gmid = "gml_id";
			    sqlite3_bind_text (stmt, 2, gmid, strlen (gmid),
					       SQLITE_STATIC);
			}
		      else
			  sqlite3_bind_text (stmt, 2, attr->attr_name,
					     strlen (attr->attr_name),
					     SQLITE_STATIC);
		  }
		else
		    sqlite3_bind_text (stmt, 2, attr->attr_name,
				       strlen (attr->attr_name), SQLITE_STATIC);
		str = "imported from XML attribute";
		sqlite3_bind_text (stmt, 3, str, strlen (str), SQLITE_STATIC);
		if (tbl->tag->tag_ns == NULL)
		  {
		      if (attr->attr_ns == NULL)
			  xml =
			      sqlite3_mprintf ("<%s %s=\"value\" />",
					       tbl->tag->tag_name,
					       attr->attr_name);
		      else
			  xml =
			      sqlite3_mprintf ("<%s %s:%s=\"value\" />",
					       tbl->tag->tag_name,
					       attr->attr_ns, attr->attr_name);
		  }
		else
		  {
		      if (attr->attr_ns == NULL)
			  xml =
			      sqlite3_mprintf ("<%s:%s %s=\"value\" />",
					       tbl->tag->tag_ns,
					       tbl->tag->tag_name,
					       attr->attr_name);
		      else
			  xml =
			      sqlite3_mprintf ("<%s:%s %s:%s=\"value\" />",
					       tbl->tag->tag_ns,
					       tbl->tag->tag_name,
					       attr->attr_ns, attr->attr_name);
		  }
		sqlite3_bind_text (stmt, 4, xml, strlen (xml), sqlite3_free);
		ret = sqlite3_step (stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      fprintf (stderr,
			       "INSERT xml_metacatalog_columns error: %s\n",
			       sqlite3_errmsg (ptr->db_handle));
		      error = 1;
		      goto stop2;
		  }
		attr = attr->next;
	    }
	  tbl = tbl->next;
      }
  stop2:
    sqlite3_finalize (stmt);

    if (error)
	return 0;
    return 1;
}

static int
do_insert (sqlite3 * db_handle, struct xml_tag *tag, const char *filename,
	   int xlink_href)
{
/* attempting to INSERT an XML Node into the appropriate table */
    struct xml_attr *attr;
    int ret;
    int pos = 1;
    sqlite3_stmt *stmt = tag->table->ins_stmt;
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    if (tag->table->parent_table != NULL)
      {
	  /* retrieving the Foreign Key value */
	  sqlite3_int64 fk_value = tag->parent->table->current;
	  sqlite3_bind_int64 (stmt, pos, fk_value);
	  pos++;
      }
    else
      {
	  /* setting the "mtdapp_filename" value */
	  sqlite3_bind_text (stmt, pos, filename, strlen (filename),
			     SQLITE_STATIC);
	  pos++;
      }
    attr = tag->first_attr;
    while (attr != NULL)
      {
	  if (xlink_href && attr->attr_value != NULL)
	    {
		/* special case: xlink_href */
		int is_xlink = 0;
		if (attr->attr_ns != NULL)
		  {
		      if (strcmp (attr->attr_ns, "xlink") == 0)
			  is_xlink = 1;
		  }
		if (is_xlink && strcmp (attr->attr_name, "href") == 0)
		  {
		      if (*(attr->attr_value) == '#')
			{
			    /* skipping the initial # "anchor" marker */
			    sqlite3_bind_text (stmt, pos, attr->attr_value + 1,
					       strlen (attr->attr_value) - 1,
					       SQLITE_STATIC);
			    pos++;
			    attr = attr->next;
			}
		      continue;
		  }
	    }
	  if (attr->attr_value == NULL)
	      sqlite3_bind_null (stmt, pos);
	  else
	      sqlite3_bind_text (stmt, pos, attr->attr_value,
				 strlen (attr->attr_value), SQLITE_STATIC);
	  pos++;
	  attr = attr->next;
      }
/* INSERTing */
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	tag->table->current = sqlite3_last_insert_rowid (db_handle);
    else
      {
	  fprintf (stderr, "INSERT error: %s\n", sqlite3_errmsg (db_handle));
	  return 0;
      }
    return 1;
}

static int
do_update (sqlite3 * db_handle, struct xml_tag *tag, const char *value, int len)
{
/* attempting to UPDATE an XML Node by setting the node's value */
    int ret;
    sqlite3_stmt *stmt = tag->table->upd_stmt;
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, value, len, SQLITE_STATIC);
    sqlite3_bind_int64 (stmt, 2, tag->table->current);
/* UPDATing */
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  fprintf (stderr, "UPDATE error: %s\n", sqlite3_errmsg (db_handle));
	  return 0;
      }
    return 1;
}

static int
do_update_with_geom (sqlite3 * db_handle, struct xml_tag *tag,
		     const char *value, int len)
{
/* attempting to UPDATE an XML Node by setting the node's value */
    int ret;
    gaiaGeomCollPtr geo;
    unsigned char *gml_expr;
    sqlite3_stmt *stmt = tag->table->upd_stmt;
    sqlite3_reset (stmt);
    sqlite3_clear_bindings (stmt);
    sqlite3_bind_text (stmt, 1, value, len, SQLITE_STATIC);
    gml_expr = malloc (len + 1);
    memcpy (gml_expr, value, len);
    *(gml_expr + len) = '\0';
    geo = gaiaParseGml (gml_expr, db_handle);
    free (gml_expr);
    if (geo == NULL)
	sqlite3_bind_null (stmt, 2);
    else
      {
	  int blob_len;
	  unsigned char *blob;
	  gaiaToSpatiaLiteBlobWkb (geo, &blob, &blob_len);
	  gaiaFreeGeomColl (geo);
	  sqlite3_bind_blob (stmt, 2, blob, blob_len, free);
      }
    sqlite3_bind_int64 (stmt, 3, tag->table->current);
/* UPDATing */
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  fprintf (stderr, "UPDATE error: %s\n", sqlite3_errmsg (db_handle));
	  return 0;
      }
    return 1;
}

static void
start_tag_2 (void *data, const char *el, const char **attr)
{
/* XML element starting - Pass II */
    const char **attrib = attr;
    int count = 0;
    const char *k;
    const char *v;
    char *ns = NULL;
    char *name = NULL;
    char *full_name = NULL;
    struct xml_params *params = (struct xml_params *) data;
    struct xml_tag *tag;
    if (params->CollapsingGML)
      {
	  /* collapsing GML Geometries */
	  gmlDynBufferAppend (params->CollapsedGML, "<", 1);
	  gmlDynBufferAppend (params->CollapsedGML, el, strlen (el));
	  while (*attrib != NULL)
	    {
		if ((count % 2) == 0)
		    k = *attrib;
		else
		  {
		      v = *attrib;
		      gmlDynBufferAppend (params->CollapsedGML, " ", 1);
		      gmlDynBufferAppend (params->CollapsedGML, k, strlen (k));
		      gmlDynBufferAppend (params->CollapsedGML, "=\"", 2);
		      gmlDynBufferAppend (params->CollapsedGML, v, strlen (v));
		      gmlDynBufferAppend (params->CollapsedGML, "\"", 1);
		  }
		attrib++;
		count++;
	    }
	  gmlDynBufferAppend (params->CollapsedGML, ">", 1);
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    split_namespace (el, &ns, &name);
    if (params->collapsed_gml)
      {
	  /* attempting to collapse GML Geometries */
	  if (is_gml_geometry (ns, name))
	    {
		if (ns != NULL)
		    free (ns);
		if (name != NULL)
		    free (name);
		if (params->CollapsedGML != NULL)
		    gmlDynBufferDestroy (params->CollapsedGML);
		params->CollapsedGML = gmlDynBufferAlloc ();
		gmlDynBufferAppend (params->CollapsedGML, "<", 1);
		gmlDynBufferAppend (params->CollapsedGML, el, strlen (el));
		while (*attrib != NULL)
		  {
		      if ((count % 2) == 0)
			  k = *attrib;
		      else
			{
			    v = *attrib;
			    gmlDynBufferAppend (params->CollapsedGML, " ", 1);
			    gmlDynBufferAppend (params->CollapsedGML, k,
						strlen (k));
			    gmlDynBufferAppend (params->CollapsedGML, "=\"", 2);
			    gmlDynBufferAppend (params->CollapsedGML, v,
						strlen (v));
			    gmlDynBufferAppend (params->CollapsedGML, "\"", 1);
			}
		      attrib++;
		      count++;
		  }
		gmlDynBufferAppend (params->CollapsedGML, ">", 1);
		params->CollapsingGML = 1;
		if (params->CollapsedGMLMarker != NULL)
		    sqlite3_free (params->CollapsedGMLMarker);
		params->CollapsedGMLMarker = sqlite3_mprintf ("%s", el);
		*(params->CharData) = '\0';
		params->CharDataLen = 0;
		return;
	    }
      }
    full_name = build_full_name (params->stack, name);
    tag = find_tag (params, ns, name, full_name);
    sqlite3_free (full_name);
    reset_tag_attributes (tag);
    push_stack (params, ns, name);
    while (*attrib != NULL)
      {
	  if ((count % 2) == 0)
	      k = *attrib;
	  else
	    {
		v = *attrib;
		split_namespace (k, &ns, &name);
		set_attribute_value (tag, ns, name, v);
		if (ns != NULL)
		    free (ns);
		if (name != NULL)
		    free (name);
	    }
	  attrib++;
	  count++;
      }
    *(params->CharData) = '\0';
    params->CharDataLen = 0;
    if (!do_insert
	(params->db_handle, tag, params->filename, params->xlink_href))
	params->db_error = 1;
}

static void
end_tag_2 (void *data, const char *el)
{
/* XML element ending - Pass II */
    struct xml_params *params = (struct xml_params *) data;
    if (params->CollapsingGML)
      {
	  /* collapsing GML Geometries */
	  if (valid_char_data (params))
	      gmlDynBufferAppend (params->CollapsedGML, params->CharData,
				  params->CharDataLen);
	  gmlDynBufferAppend (params->CollapsedGML, "</", 2);
	  gmlDynBufferAppend (params->CollapsedGML, el, strlen (el));
	  gmlDynBufferAppend (params->CollapsedGML, ">", 1);
	  if (strcmp (params->CollapsedGMLMarker, el) == 0)
	    {
		sqlite3_free (params->CollapsedGMLMarker);
		params->CollapsedGMLMarker = NULL;
		params->CollapsingGML = 0;
	    }
	  *(params->CharData) = '\0';
	  params->CharDataLen = 0;
	  return;
      }
    if (params->CollapsedGML != NULL)
      {
	  /* updating the Node value - collapsed GML */
	  char *full_name =
	      build_full_name (params->stack->prev, params->stack->tag_name);
	  struct xml_tag *tag =
	      find_tag (params, params->stack->tag_ns, params->stack->tag_name,
			full_name);
	  sqlite3_free (full_name);
	  if (!do_update_with_geom
	      (params->db_handle, tag, params->CollapsedGML->Buffer,
	       params->CollapsedGML->WriteOffset))
	      params->db_error = 1;
	  gmlDynBufferDestroy (params->CollapsedGML);
	  params->CollapsedGML = NULL;
	  goto done;
      }
    if (valid_char_data (params))
      {
	  /* updating the Node value */
	  char *full_name =
	      build_full_name (params->stack->prev, params->stack->tag_name);
	  struct xml_tag *tag =
	      find_tag (params, params->stack->tag_ns, params->stack->tag_name,
			full_name);
	  sqlite3_free (full_name);
	  if (!do_update
	      (params->db_handle, tag, params->CharData, params->CharDataLen))
	      params->db_error = 1;
      }
  done:
    *(params->CharData) = '\0';
    params->CharDataLen = 0;
    pop_stack (params);
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

    *handle = NULL;
    printf ("SQLite version: %s\n", sqlite3_libversion ());
    printf ("SpatiaLite version: %s\n", spatialite_version ());

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

#ifndef timersub
/* This is a copy from GNU C Library (GNU LGPL 2.1), sys/time.h. */
#define timersub(a, b, result)                                               \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)
#endif

static void
format_elapsed_time (struct timeval *start, struct timeval *stop, char *elapsed)
{
/* well formatting elapsed time */
    struct timeval diff;
    int x;
    int millis;
    int secs;
    int mins;
    int hh;

    timersub (stop, start, &diff);
    millis = diff.tv_usec / 1000;
    secs = diff.tv_sec % 60;
    x = diff.tv_sec / 60;
    mins = x % 60;
    x /= 60;
    hh = x;
    sprintf (elapsed, "%d:%02d:%02d.%03d", hh, mins, secs, millis);
}

static void
do_update_progress (int phase, sqlite3_int64 totlen, sqlite3_int64 donelen)
{
/* updating the "running wheel" */
    char progress[64];
    strcpy (progress, "........................................");
    if (donelen)
      {
	  double percent = (double) donelen / (double) totlen;
	  int tics = 40.0 * percent;
	  int i;
	  for (i = 0; i < tics; i++)
	      progress[i] = 'X';
      }
    switch (phase % 8)
      {
      case 0:
	  printf ("| %s\r", progress);
	  break;
      case 1:
	  printf ("/ %s\r", progress);
	  break;
      case 2:
	  printf ("- %s\r", progress);
	  break;
      case 3:
	  printf ("\\ %s\r", progress);
	  break;
      case 4:
	  printf ("| %s\r", progress);
	  break;
      case 5:
	  printf ("/ %s\r", progress);
	  break;
      case 6:
	  printf ("- %s\r", progress);
	  break;
      case 7:
	  printf ("\\ %s\r", progress);
	  break;
      };
    fflush (stdout);
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_xml_load ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr, "-x or --xml-path pathname       the XML file path\n");
    fprintf (stderr,
	     "-d or --db-path     pathname    the SpatiaLite DB path\n\n");
    fprintf (stderr, "you can specify the following options as well\n");
    fprintf (stderr,
	     "-cg or --collapsed-gml          collapsed GML Geometries\n");
    fprintf (stderr,
	     "-xl or --xlink-href             special GML xlink:href handling\n");
    fprintf (stderr,
	     "-nl or --nl-level      num      tree-level for table-names (dft: 0)\n");
    fprintf (stderr,
	     "-pl or --parent-levels num      how many ancestors for table-names (dft: -1)\n");
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
    int next_arg = ARG_NONE;
    const char *xml_path = NULL;
    const char *db_path = NULL;
    int name_level = 0;
    int parent_levels = -1;
    int in_memory = 0;
    int cache_size = 0;
    int journal_off = 0;
    int collapsed_gml = 0;
    int xlink_href = 0;
    int error = 0;
    char Buff[BUFFSIZE];
    int done = 0;
    int len;
    XML_Parser parser = NULL;
    FILE *xml_file;
    struct xml_params params;
    void *cache;
    struct timeval start;
    struct timeval stop;
    char elapsed[64];
    int phase = 0;
    int blklen = 0;
    sqlite3_int64 totlen = 0;
    sqlite3_int64 donelen = 0;

    gettimeofday (&start, NULL);

    params.filename = NULL;
    params.db_handle = NULL;
    params.journal_off = 0;
    params.collapsed_gml = 0;
    params.xlink_href = 0;
    params.CharDataStep = 65536;
    params.CharDataMax = params.CharDataStep;
    params.CharData = malloc (params.CharDataStep);
    params.CharDataLen = 0;
    params.parse_error = 0;
    params.db_error = 0;
    params.first_tag = NULL;
    params.last_tag = NULL;
    params.sort_array = NULL;
    params.count_array = 0;
    params.first_table = NULL;
    params.last_table = NULL;
    params.stack = NULL;
    params.CollapsingGML = 0;
    params.CollapsedGML = NULL;
    params.CollapsedGMLMarker = NULL;
    params.treeLevel = -1;

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
		  case ARG_NAME_LEVEL:
		      name_level = atoi (argv[i]);
		      break;
		  case ARG_PARENT_LEVELS:
		      parent_levels = atoi (argv[i]);
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
	  if (strcasecmp (argv[i], "--name-level") == 0
	      || strcmp (argv[i], "-nl") == 0)
	    {
		next_arg = ARG_NAME_LEVEL;
		continue;
	    }
	  if (strcasecmp (argv[i], "--parent-levels") == 0
	      || strcmp (argv[i], "-pl") == 0)
	    {
		next_arg = ARG_PARENT_LEVELS;
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
	  if (strcasecmp (argv[i], "-cg") == 0)
	    {
		collapsed_gml = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--collapsed-gml") == 0)
	    {
		collapsed_gml = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "-xl") == 0)
	    {
		xlink_href = 1;
		next_arg = ARG_NONE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--xlink-href") == 0)
	    {
		xlink_href = 1;
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
    if (in_memory)
	cache_size = 0;
    cache = spatialite_alloc_connection ();
    open_db (db_path, &handle, cache_size, cache);
    if (!handle)
	return -1;

    printf ("Target DB: %s\n", db_path);

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
    params.journal_off = journal_off;
    params.collapsed_gml = collapsed_gml;
    params.xlink_href = xlink_href;
    set_xml_filename (&params, xml_path);

/* XML parsing - pass I */
    xml_file = fopen (xml_path, "rb");
    if (!xml_file)
      {
	  fprintf (stderr, "cannot open %s\n", xml_path);
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

    printf ("Input XML: %s\n", xml_path);

    XML_SetUserData (parser, &params);
/* XML parsing - pass I */
    XML_SetElementHandler (parser, start_tag_1, end_tag_1);
    XML_SetCharacterDataHandler (parser, xmlCharData);
    blklen = 0;
    while (!done)
      {
	  if (params.parse_error || params.db_error)
	      goto parser_error1;
	  len = fread (Buff, 1, BUFFSIZE, xml_file);
	  totlen += len;
	  blklen += len;
	  if (blklen > BUFFSIZE * 100)
	    {
		do_update_progress (phase++, totlen, donelen);
		blklen -= BUFFSIZE * 100;
	    }
	  if (ferror (xml_file))
	    {
		fprintf (stderr, "XML Read error\n");
		goto parser_error1;
	    }
	  done = feof (xml_file);
	  if (!XML_Parse (parser, Buff, len, done))
	    {
		fprintf (stderr, "Parse error at line %d:\n%s\n",
			 (int) XML_GetCurrentLineNumber (parser),
			 XML_ErrorString (XML_GetErrorCode (parser)));
		goto parser_error1;
	    }
      }
  parser_error1:
    XML_ParserFree (parser);
    fclose (xml_file);

/* assigning the SQL table names for each XML Node class */
    set_unique_names (&params, name_level, parent_levels);
    set_table_names (&params);

    if (params.journal_off)
      {
	  /* disabling the Journal File */
	  ret =
	      sqlite3_exec (params.db_handle, "PRAGMA journal_mode=OFF", NULL,
			    NULL, &err_msg);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "JOURNAL MODE=OFF error: %s\n", err_msg);
		sqlite3_free (err_msg);
		goto abort;
	    }
	  ret =
	      sqlite3_exec (params.db_handle, "PRAGMA synchronous=OFF", NULL,
			    NULL, &err_msg);
	  if (ret != SQLITE_OK)
	    {
		fprintf (stderr, "SYNCHRONOUS=OFF error: %s\n", err_msg);
		sqlite3_free (err_msg);
		goto abort;
	    }
      }

/* starting a transaction */
    ret = sqlite3_exec (params.db_handle, "BEGIN", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "BEGIN error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }

/* creating/updating the SQL tables */
    if (!prepare_sql_tables (&params))
	goto abort;

/* XML parsing - pass II */
    done = 0;
    if (params.CollapsedGMLMarker != NULL)
      {
	  sqlite3_free (params.CollapsedGMLMarker);
	  params.CollapsedGMLMarker = NULL;
      }
    if (params.CollapsedGML != NULL)
      {
	  gmlDynBufferDestroy (params.CollapsedGML);
	  params.CollapsedGML = NULL;
      }
    params.CollapsingGML = 0;

    xml_file = fopen (xml_path, "rb");
    if (!xml_file)
      {
	  fprintf (stderr, "cannot open %s\n", xml_path);
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
/* XML parsing - pass II */
    XML_SetElementHandler (parser, start_tag_2, end_tag_2);
    XML_SetCharacterDataHandler (parser, xmlCharData);
    blklen = 0;
    while (!done)
      {
	  if (params.parse_error || params.db_error)
	      goto parser_error2;
	  len = fread (Buff, 1, BUFFSIZE, xml_file);
	  donelen += len;
	  blklen += len;
	  if (blklen > BUFFSIZE * 10)
	    {
		do_update_progress (phase++, totlen, donelen);
		blklen -= BUFFSIZE * 10;
	    }
	  if (ferror (xml_file))
	    {
		fprintf (stderr, "XML Read error\n");
		goto parser_error2;
	    }
	  done = feof (xml_file);
	  if (!XML_Parse (parser, Buff, len, done))
	    {
		fprintf (stderr, "Parse error at line %d:\n%s\n",
			 (int) XML_GetCurrentLineNumber (parser),
			 XML_ErrorString (XML_GetErrorCode (parser)));
		goto parser_error2;
	    }
      }
  parser_error2:
    XML_ParserFree (parser);
    fclose (xml_file);

/* committing the transaction */
    ret = sqlite3_exec (params.db_handle, "COMMIT", NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "COMMIT error: %s\n", err_msg);
	  sqlite3_free (err_msg);
      }

    gettimeofday (&stop, NULL);
    format_elapsed_time (&start, &stop, elapsed);
    printf ("Done - inserted/updated rows: %d [%s]\n\n",
	    sqlite3_total_changes (params.db_handle), elapsed);

  abort:
    params_cleanup (&params);

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
