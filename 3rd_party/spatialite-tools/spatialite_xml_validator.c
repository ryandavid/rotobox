/* 
/ spatialite_xml_validator
/
/ a tool performing XML schema validation 
/
/ version 1.0, 2014 October 28
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

struct list_item
{
/* a file path to be validated */
    char *path;
    struct list_item *next;
};

struct list_of_files
{
/* a list of files */
    struct list_item *first;
    struct list_item *last;
};

struct schema_cached_item
{
/* a cached XSD item */
    char *schemaURI;
    xmlDocPtr schema_doc;
    xmlSchemaParserCtxtPtr parser_ctxt;
    xmlSchemaPtr schema;
    struct schema_cached_item *next;
};

struct schema_cache
{
/* a cache storing XSD schemata */
    struct schema_cached_item *first;
    struct schema_cached_item *last;
};

struct ns_vxpath
{
/* a Namespace definition */
    char *Prefix;
    char *Href;
    struct ns_vxpath *Next;
};

struct namespaces_vxpath
{
/* Namespace container */
    struct ns_vxpath *First;
    struct ns_vxpath *Last;
};

static void
add_cached_xsd (struct schema_cache *cache, const char *schemaURI,
		xmlDocPtr schema_doc, xmlSchemaParserCtxtPtr parser_ctxt,
		xmlSchemaPtr schema)
{
/* adding an XSD item to the cache */
    int len;
    struct schema_cached_item *p = malloc (sizeof (struct schema_cached_item));
    len = strlen (schemaURI);
    p->schemaURI = malloc (len + 1);
    strcpy (p->schemaURI, schemaURI);
    p->schema_doc = schema_doc;
    p->parser_ctxt = parser_ctxt;
    p->schema = schema;
    p->next = NULL;
    if (cache->first == NULL)
	cache->first = p;
    if (cache->last != NULL)
	cache->last->next = p;
    cache->last = p;
}

static void
cache_cleanup (struct schema_cache *cache)
{
/* memory cleanup - freeing the XSD cache */
    struct schema_cached_item *p;
    struct schema_cached_item *pn;
    p = cache->first;
    while (p != NULL)
      {
	  pn = p->next;
	  free (p->schemaURI);
	  xmlSchemaFreeParserCtxt (p->parser_ctxt);
	  xmlSchemaFree (p->schema);
	  xmlFreeDoc (p->schema_doc);
	  free (p);
	  p = pn;
      }
}

static struct schema_cached_item *
find_cached_schema (struct schema_cache *cache, const char *schemaURI)
{
/* attempting to retrieve a cached XSD */
    struct schema_cached_item *p = cache->first;
    while (p != NULL)
      {
	  if (strcmp (p->schemaURI, schemaURI) == 0)
	      return p;
	  p = p->next;
      }
    return NULL;
}

static void
list_cleanup (struct list_of_files *list)
{
/* memory cleanup - freeing the list */
    struct list_item *item;
    struct list_item *item_n;
    item = list->first;
    while (item != NULL)
      {
	  item_n = item->next;
	  free (item->path);
	  free (item);
	  item = item_n;
      }
    free (list);
}

static void
clean_cr (char *path)
{
/* removing trailing CR/LF characters */
    int len = strlen (path);
    char *p = path + len - 1;
    while (p > path)
      {
	  if (*p == '\r' || *p == '\n')
	      *p-- = '\0';
	  else
	      break;
      }
}

static char *
my_getline (FILE * in)
{
/* reading a whole line */
    int c;
    char *buf = malloc (1024 * 1024);
    char *p = buf;
    while ((c = getc (in)) != EOF)
      {
	  *p++ = c;
	  if (c == '\n')
	      break;
      }
    if (p == buf)
      {
	  free (buf);
	  return NULL;
      }
    *p = '\0';
    return buf;
}

static struct list_of_files *
parse_list (const char *path)
{
/* parsing a file containing a list of file-paths */
    char *line = NULL;
    int count = 0;
    struct list_item *item;
    struct list_of_files *list = malloc (sizeof (struct list_of_files));
    FILE *in = fopen (path, "rb");
    if (in == NULL)
	return NULL;
    list->first = NULL;
    list->last = NULL;

    while ((line = my_getline (in)) != NULL)
      {
	  int len;
	  item = malloc (sizeof (struct list_item));
	  clean_cr (line);
	  len = strlen (line);
	  item->path = malloc (len + 1);
	  strcpy (item->path, line);
	  item->next = NULL;
	  if (list->first == NULL)
	      list->first = item;
	  if (list->last != NULL)
	      list->last->next = item;
	  list->last = item;
	  free (line);
	  line = NULL;
      }

/* final check */
    item = list->first;
    while (item != NULL)
      {
	  count++;
	  item = item->next;
      }
    if (count == 0)
      {
	  list_cleanup (list);
	  return NULL;
      }
    return list;
}

static void
free_vxpath_ns (struct ns_vxpath *ns)
{
/* memory cleanup - destroying a Namespace item */
    if (!ns)
	return;
    if (ns->Prefix)
	free (ns->Prefix);
    if (ns->Href)
	free (ns->Href);
    free (ns);
}

static void
free_vxpath_namespaces (struct namespaces_vxpath *ns_list)
{
/* memory cleanup - destroying the Namespaces list */
    struct ns_vxpath *ns;
    struct ns_vxpath *nns;
    if (!ns_list)
	return;
    ns = ns_list->First;
    while (ns)
      {
	  nns = ns->Next;
	  free_vxpath_ns (ns);
	  ns = nns;
      }
    free (ns_list);
}

static void
add_vxpath_ns (struct namespaces_vxpath *ns_list, const char *prefix,
	       const char *href)
{
/* inserting a further Namespace into the list */
    int len;
    struct ns_vxpath *ns = ns_list->First;
    while (ns)
      {
	  /* checking if it's already defined */
	  if (ns->Prefix == NULL || prefix == NULL)
	    {
		if (ns->Prefix == NULL && prefix == NULL
		    && strcmp (ns->Href, href) == 0)
		  {
		      /* ok, already defined (default Namespace) */
		      return;
		  }
	    }
	  else
	    {
		if (strcmp (ns->Prefix, prefix) == 0
		    && strcmp (ns->Href, href) == 0)
		  {
		      /* ok, already defined */
		      return;
		  }
	    }
	  ns = ns->Next;
      }

/* inserting a new Namespace */
    ns = malloc (sizeof (struct ns_vxpath));
    if (prefix == NULL)
	ns->Prefix = NULL;
    else
      {
	  len = strlen (prefix);
	  ns->Prefix = malloc (len + 1);
	  strcpy (ns->Prefix, prefix);
      }
    len = strlen (href);
    ns->Href = malloc (len + 1);
    strcpy (ns->Href, href);
    ns->Next = NULL;
    if (ns_list->First == NULL)
	ns_list->First = ns;
    if (ns_list->Last != NULL)
	ns_list->Last->Next = ns;
    ns_list->Last = ns;
}

static void
feed_vxpath_ns (struct namespaces_vxpath *ns_list, xmlNodePtr start)
{
/* recursively searching for Namespaces */
    xmlNodePtr node = start;
    while (node)
      {
	  if (node->ns != NULL)
	    {
		/* a Namespace is defined */
		add_vxpath_ns (ns_list, (const char *) (node->ns->prefix),
			       (const char *) (node->ns->href));
	    }
	  if (node->properties != NULL)
	    {
		/* exploring the Attribute list */
		struct _xmlAttr *attr = node->properties;
		while (attr)
		  {
		      if (attr->type == XML_ATTRIBUTE_NODE)
			{
			    if (attr->ns != NULL)
			      {
				  /* a Namespace is defined */
				  add_vxpath_ns (ns_list,
						 (const char *) (attr->
								 ns->prefix),
						 (const char *) (attr->
								 ns->href));
			      }
			}
		      attr = attr->next;
		  }
	    }
	  feed_vxpath_ns (ns_list, node->children);
	  node = node->next;
      }
}

static struct namespaces_vxpath *
get_vxpath_namespaces (void *p_xml_doc)
{
/* creating and populating the Namespaces list */
    xmlDocPtr xml_doc = (xmlDocPtr) p_xml_doc;
    xmlNodePtr root = xmlDocGetRootElement (xml_doc);
    struct namespaces_vxpath *ns_list;
    ns_list = malloc (sizeof (struct namespaces_vxpath));
    ns_list->First = NULL;
    ns_list->Last = NULL;
    feed_vxpath_ns (ns_list, root);
    return ns_list;
}

static int
eval_vxpath_expr (xmlDocPtr xml_doc, const char *xpath_expr,
		  xmlXPathContextPtr * p_xpathCtx,
		  xmlXPathObjectPtr * p_xpathObj)
{
/* evaluating an XPath expression */
    xmlXPathObjectPtr xpathObj;
    xmlXPathContextPtr xpathCtx;
/* attempting to identify all required Namespaces */
    struct ns_vxpath *ns;
    struct namespaces_vxpath *ns_list = get_vxpath_namespaces (xml_doc);
/* creating an XPath context */
    xpathCtx = xmlXPathNewContext (xml_doc);
    if (xpathCtx == NULL)
	return 0;
/* registering all Namespaces */
    if (xpathCtx != NULL && ns_list != NULL)
      {
	  ns = ns_list->First;
	  while (ns)
	    {
		if (ns->Prefix == NULL)
		  {
		      /* the default Namespace always is "dflt:xx" */
		      xmlXPathRegisterNs (xpathCtx, (xmlChar *) "dflt",
					  (xmlChar *) ns->Href);
		  }
		else
		  {
		      /* a fully qualified Namespace */
		      xmlXPathRegisterNs (xpathCtx, (xmlChar *) ns->Prefix,
					  (xmlChar *) ns->Href);
		  }
		ns = ns->Next;
	    }
      }
    free_vxpath_namespaces (ns_list);
/* evaluating the XPath expression */
    xpathObj = xmlXPathEvalExpression ((const xmlChar *) xpath_expr, xpathCtx);
    if (xpathObj != NULL)
      {
	  xmlNodeSetPtr nodes = xpathObj->nodesetval;
	  int num_nodes = (nodes) ? nodes->nodeNr : 0;
	  if (num_nodes >= 1)
	    {
		/* OK: match found */
		*p_xpathCtx = xpathCtx;
		*p_xpathObj = xpathObj;
		xmlSetGenericErrorFunc ((void *) stderr, NULL);
		return 1;
	    }
	  /* invalid: empty nodeset */
	  xmlXPathFreeObject (xpathObj);
      }
    xmlXPathFreeContext (xpathCtx);
    xmlSetGenericErrorFunc ((void *) stderr, NULL);
    return 0;
}

static char *
get_schema_uri (xmlDocPtr xml_doc)
{
/* Return the internally defined SchemaURI from a valid XmlDocument */
    char *uri = NULL;
    xmlXPathContextPtr xpathCtx;
    xmlXPathObjectPtr xpathObj;
/* retrieving the XMLDocument internal SchemaURI (if any) */
    if (eval_vxpath_expr
	(xml_doc, "/*/@xsi:schemaLocation", &xpathCtx, &xpathObj))
      {
	  /* attempting first to extract xsi:schemaLocation */
	  xmlNodeSetPtr nodeset = xpathObj->nodesetval;
	  xmlNodePtr node;
	  int num_nodes = (nodeset) ? nodeset->nodeNr : 0;
	  if (num_nodes == 1)
	    {
		node = nodeset->nodeTab[0];
		if (node->type == XML_ATTRIBUTE_NODE)
		  {
		      if (node->children != NULL)
			{
			    if (node->children->content != NULL)
			      {
				  const char *str =
				      (const char *) (node->children->content);
				  const char *ptr = str;
				  int i;
				  int len = strlen (str);
				  for (i = len - 1; i >= 0; i--)
				    {
					if (*(str + i) == ' ')
					  {
					      /* last occurrence of SPACE [namespace/schema separator] */
					      ptr = str + i + 1;
					      break;
					  }
				    }
				  len = strlen (ptr);
				  uri = malloc (len + 1);
				  strcpy (uri, ptr);
			      }
			}
		  }
	    }
	  if (uri != NULL)
	      xmlXPathFreeContext (xpathCtx);
	  xmlXPathFreeObject (xpathObj);
      }
    if (uri == NULL)
      {
	  /* checking for xsi:noNamespaceSchemaLocation */
	  if (eval_vxpath_expr
	      (xml_doc, "/*/@xsi:noNamespaceSchemaLocation", &xpathCtx,
	       &xpathObj))
	    {
		xmlNodeSetPtr nodeset = xpathObj->nodesetval;
		xmlNodePtr node;
		int num_nodes = (nodeset) ? nodeset->nodeNr : 0;
		if (num_nodes == 1)
		  {
		      node = nodeset->nodeTab[0];
		      if (node->type == XML_ATTRIBUTE_NODE)
			{
			    if (node->children != NULL)
			      {
				  if (node->children->content != NULL)
				    {
					int len =
					    strlen ((const char *)
						    node->children->content);
					uri = malloc (len + 1);
					strcpy (uri,
						(const char *) node->
						children->content);
				    }
			      }
			}
		  }
		xmlXPathFreeContext (xpathCtx);
		xmlXPathFreeObject (xpathObj);
	    }
      }

    return uri;
}

static int
validate_xml_document (struct schema_cache *cache, const char *path)
{
/* validating a single XML document */
    char *schemaURI = NULL;
    xmlDocPtr xml_doc = NULL;
    xmlSchemaPtr schema = NULL;
    xmlSchemaValidCtxtPtr valid_ctxt = NULL;
    xmlDocPtr xschema_doc = NULL;
    xmlSchemaPtr xschema = NULL;
    xmlSchemaParserCtxtPtr xparser_ctxt = NULL;
    struct schema_cached_item *saved;
    int new_schema;
    fprintf (stderr, "validating %s\n", path);
    fprintf (stderr, "----------------------------------------------------\n");
    fprintf (stderr, "step #1) checking if the XML document is well formed\n");
/* testing if the XMLDocument is well-formed */
    xml_doc = xmlReadFile (path, NULL, XML_PARSE_BIG_LINES);
    if (xml_doc == NULL)
      {
	  /* parsing error; not a well-formed XML */
	  fprintf (stderr, "XML parsing error\n");
	  goto error;
      }
    fprintf (stderr, "\tYES: well-formedness confirmed\n");
    fprintf (stderr,
	     "step #2) attempting to identify the Schema Definition URI\n");
/* attempting to extract the Schema URI from the XML document itself */
    schemaURI = get_schema_uri (xml_doc);
    if (schemaURI == NULL)
      {
	  fprintf (stderr,
		   "the XML Document does not contains any Schema declaration\n");
	  goto error;
      }
    fprintf (stderr, "\tXSD-URI: %s\n", schemaURI);
    saved = find_cached_schema (cache, schemaURI);
    if (saved == NULL)
      {
	  /* not already cache XSD */
	  new_schema = 1;
	  fprintf (stderr,
		   "step #3) attempting to load and parse the XML Schema Definition (XSD)\n");
/* preparing the XML Schema */
	  xschema_doc = xmlReadFile (schemaURI, NULL, XML_PARSE_BIG_LINES);
	  if (xschema_doc == NULL)
	    {
		fprintf (stderr, "unable to load the XML Schema\n");
		goto error;
	    }
	  xparser_ctxt = xmlSchemaNewDocParserCtxt (xschema_doc);
	  if (xparser_ctxt == NULL)
	    {
		fprintf (stderr, "unable to prepare the XML Schema Context\n");
		goto error;
	    }
	  xschema = xmlSchemaParse (xparser_ctxt);
	  if (xschema == NULL)
	    {
		fprintf (stderr, "invalid XML Schema\n");
		goto error;
	    }
	  fprintf (stderr, "\tXSD ready\n");
	  if (new_schema)
	    {
		/* saving the schema into the cache */
		add_cached_xsd (cache, schemaURI, xschema_doc, xparser_ctxt,
				xschema);
		xschema_doc = NULL;
		xparser_ctxt = NULL;
		schema = xschema;
		xschema = NULL;
	    }
      }
    else
      {
	  /* already cached XSD */
	  fprintf (stderr, "step #3) already cached XSD\n");
	  schema = saved->schema;
      }

    fprintf (stderr,
	     "step #4) attempting to validate for Schema conformance\n");
/* Schema validation */
    valid_ctxt = xmlSchemaNewValidCtxt (schema);
    if (valid_ctxt == NULL)
      {
	  fprintf (stderr, "unable to prepare a validation context\n");
	  goto error;
      }
    if (xmlSchemaValidateDoc (valid_ctxt, xml_doc) != 0)
      {
	  fprintf (stderr, "Schema validation failed\n");
	  goto error;
      }
    fprintf (stderr, "\tOK: XML Schema validation successfully verified\n\n");
    xmlSchemaFreeValidCtxt (valid_ctxt);
    xmlFreeDoc (xml_doc);
    free (schemaURI);
    return 1;
  error:
    if (valid_ctxt != NULL)
	xmlSchemaFreeValidCtxt (valid_ctxt);
    if (xparser_ctxt != NULL)
	xmlSchemaFreeParserCtxt (xparser_ctxt);
    if (xschema != NULL)
	xmlSchemaFree (xschema);
    if (xschema_doc != NULL)
	xmlFreeDoc (xschema_doc);
    if (xml_doc != NULL)
	xmlFreeDoc (xml_doc);
    if (schemaURI != NULL)
	free (schemaURI);
    fprintf (stderr, "ERROR - invalid XML: %s\n\n", path);
    return 0;
}

int
main (int argc, char *argv[])
{
/* the MAIN function mainly perform arguments checking */
    const char *xml_path = NULL;
    const char *list_path = NULL;
    int single = 0;
    int list = 0;
    int err = 1;
    int valids = 0;
    int invalids = 0;
    struct schema_cache cache;
    cache.first = NULL;
    cache.last = NULL;
    if (argc == 3)
      {
	  if (strcmp (argv[1], "-f") == 0)
	    {
		single = 1;
		xml_path = argv[2];
		err = 0;
	    }
	  else if (strcmp (argv[1], "-l") == 0)
	    {
		list = 1;
		list_path = argv[2];
		err = 0;
	    }
      }
    if (err)
      {
	  /* printing the argument list and quitting */
	  fprintf (stderr, "\n\nusage: spatialite_xml_validate -f xml-path\n");
	  fprintf (stderr,
		   "   or: spatialite_xml_validate -l list-of-paths-file\n");
	  goto error;
      }

    fprintf (stderr, "libxml2 version: %s\n\n", LIBXML_DOTTED_VERSION);
    if (single)
      {
	  int ret = validate_xml_document (&cache, xml_path);
	  if (ret)
	      valids++;
	  else
	      invalids++;
      }
    if (list)
      {
	  struct list_item *item;
	  struct list_of_files *xml_list = parse_list (list_path);
	  if (xml_list == NULL)
	    {
		fprintf (stderr, "Invalid list-file: %s\n", list_path);
		goto error;
	    }
	  item = xml_list->first;
	  while (item != NULL)
	    {
		int ret = validate_xml_document (&cache, item->path);
		if (ret)
		    valids++;
		else
		    invalids++;
		item = item->next;
	    }
	  list_cleanup (xml_list);
      }

    fprintf (stderr,
	     "====================\n  Valid XML documents: %d\nInvalid XML documents: %d\n\n",
	     valids, invalids);
    cache_cleanup (&cache);
    xmlCleanupParser ();
    return 0;
  error:
    cache_cleanup (&cache);
    xmlCleanupParser ();
    return -1;
}
