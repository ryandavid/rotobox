/* 
/ spatialite_network
/
/ an analysis / validation tool for topological networks
/
/ version 1.0, 2008 October 13
/
/ Author: Sandro Furieri a.furieri@lqt.it
/
/ Copyright (C) 2008  Alessandro Furieri
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

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp	_stricmp
#endif /* not WIN32 */

#if defined(_WIN32) || defined (__MINGW32__)
#define FORMAT_64	"%I64d"
#else
#define FORMAT_64	"%lld"
#endif

#define ARG_NONE			0
#define ARG_DB_PATH			1
#define ARG_TABLE			2
#define ARG_FROM_COLUMN		3
#define ARG_TO_COLUMN		4
#define ARG_COST_COLUMN		5
#define ARG_GEOM_COLUMN		6
#define ARG_NAME_COLUMN		7
#define ARG_ONEWAY_TOFROM	8
#define ARG_ONEWAY_FROMTO	9
#define ARG_OUT_TABLE		10
#define ARG_VIRT_TABLE		11

#define MAX_BLOCK	1048576

struct pre_node
{
/* a preliminary node */
    sqlite3_int64 id;
    char code[32];
    struct pre_node *next;
};

struct node
{
/* a NODE */
    int internal_index;
    sqlite3_int64 id;
    char code[32];
    double x;
    double y;
    struct arc_ref *first_outcoming;
    struct arc_ref *last_outcoming;
    struct arc_ref *first_incoming;
    struct arc_ref *last_incoming;
    struct node *next;
};

struct arc
{
/* an ARC */
    sqlite3_int64 rowid;
    struct node *from;
    struct node *to;
    double cost;
    struct arc *next;
};

struct arc_ref
{
/* a reference to an Arc */
    struct arc *reference;
    struct arc_ref *next;
};

struct graph
{
    struct pre_node *first_pre;
    struct pre_node *last_pre;
    int n_pre_nodes;
    struct pre_node **sorted_pre_nodes;
    struct arc *first_arc;
    struct arc *last_arc;
    struct node *first_node;
    struct node *last_node;
    int n_nodes;
    struct node **sorted_nodes;
    int error;
    int node_code;
    int max_code_length;
};

static struct graph *
graph_init ()
{
/* allocates and initializes the graph structure */
    struct graph *p = malloc (sizeof (struct graph));
    p->first_pre = NULL;
    p->last_pre = NULL;
    p->n_pre_nodes = 0;
    p->sorted_pre_nodes = NULL;
    p->n_pre_nodes = 0;
    p->sorted_pre_nodes = NULL;
    p->first_arc = NULL;
    p->last_arc = NULL;
    p->first_node = NULL;
    p->last_node = NULL;
    p->n_nodes = 0;
    p->sorted_nodes = NULL;
    p->error = 0;
    p->node_code = 0;
    p->max_code_length = 0;
    return p;
}

static void
graph_free_pre (struct graph *p)
{
/* cleaning up the preliminary Nodes list */
    struct pre_node *pP;
    struct pre_node *pPn;
    if (!p)
	return;
    pP = p->first_pre;
    while (pP)
      {
	  pPn = pP->next;
	  free (pP);
	  pP = pPn;
      }
    p->first_pre = NULL;
    p->last_pre = NULL;
    p->n_pre_nodes = 0;
    if (p->sorted_pre_nodes)
	free (p->sorted_pre_nodes);
    p->sorted_pre_nodes = NULL;
}

static void
graph_free (struct graph *p)
{
/* cleaning up any memory allocation for the graph structure */
    struct arc *pA;
    struct arc *pAn;
    struct node *pN;
    struct node *pNn;
    struct arc_ref *pAR;
    struct arc_ref *pARn;
    if (!p)
	return;
    graph_free_pre (p);
    pA = p->first_arc;
    while (pA)
      {
	  pAn = pA->next;
	  free (pA);
	  pA = pAn;
      }
    pN = p->first_node;
    while (pN)
      {
	  pNn = pN->next;
	  pAR = pN->first_incoming;
	  while (pAR)
	    {
		pARn = pAR->next;
		free (pAR);
		pAR = pARn;
	    }
	  pAR = pN->first_outcoming;
	  while (pAR)
	    {
		pARn = pAR->next;
		free (pAR);
		pAR = pARn;
	    }
	  free (pN);
	  pN = pNn;
      }
    if (p->sorted_nodes)
	free (p->sorted_nodes);
    free (p);
}

static int
cmp_nodes2_code (const void *p1, const void *p2)
{
/* compares two nodes  by CODE [for BSEARCH] */
    struct node *pN1 = (struct node *) p1;
    struct node *pN2 = *((struct node **) p2);
    return strcmp (pN1->code, pN2->code);
}

static int
cmp_nodes2_id (const void *p1, const void *p2)
{
/* compares two nodes  by ID [for BSEARCH] */
    struct node *pN1 = (struct node *) p1;
    struct node *pN2 = *((struct node **) p2);
    if (pN1->id == pN2->id)
	return 0;
    if (pN1->id > pN2->id)
	return 1;
    return -1;
}

static struct node *
find_node (struct graph *p_graph, sqlite3_int64 id, const char *code)
{
/* searching a Node into the sorted list */
    struct node **ret;
    struct node pN;
    if (!(p_graph->sorted_nodes))
	return NULL;
    if (p_graph->node_code)
      {
	  /* Nodes are identified by a TEXT code */
	  int len = strlen (code);
	  if (len > 31)
	    {
		memcpy (pN.code, code, 31);
		*(pN.code + 31) = '\0';
	    }
	  else
	      strcpy (pN.code, code);
	  ret = bsearch (&pN, p_graph->sorted_nodes, p_graph->n_nodes,
			 sizeof (struct node *), cmp_nodes2_code);
      }
    else
      {
	  /* Nodes are identified by an INTEGER id */
	  pN.id = id;
	  ret = bsearch (&pN, p_graph->sorted_nodes, p_graph->n_nodes,
			 sizeof (struct node *), cmp_nodes2_id);
      }
    if (!ret)
	return NULL;
    return *ret;
}

static int
cmp_nodes1_code (const void *p1, const void *p2)
{
/* compares two nodes  by CODE [for QSORT] */
    struct node *pN1 = *((struct node **) p1);
    struct node *pN2 = *((struct node **) p2);
    return strcmp (pN1->code, pN2->code);
}

static int
cmp_nodes1_id (const void *p1, const void *p2)
{
/* compares two nodes  by ID [for QSORT] */
    struct node *pN1 = *((struct node **) p1);
    struct node *pN2 = *((struct node **) p2);
    if (pN1->id == pN2->id)
	return 0;
    if (pN1->id > pN2->id)
	return 1;
    return -1;
}

static void
sort_nodes (struct graph *p_graph)
{
/* updating the Nodes sorted list */
    int i;
    struct node *pN;
    p_graph->n_nodes = 0;
    if (p_graph->sorted_nodes)
      {
	  /* we must free the already existent sorted list */
	  free (p_graph->sorted_nodes);
      }
    p_graph->sorted_nodes = NULL;
    pN = p_graph->first_node;
    while (pN)
      {
	  (p_graph->n_nodes)++;
	  pN = pN->next;
      }
    if (!(p_graph->n_nodes))
	return;
    p_graph->sorted_nodes = malloc (sizeof (struct node *) * p_graph->n_nodes);
    i = 0;
    pN = p_graph->first_node;
    while (pN)
      {
	  *(p_graph->sorted_nodes + i++) = pN;
	  pN = pN->next;
      }
    if (p_graph->node_code)
      {
	  /* Nodes are identified by a TEXT code */
	  qsort (p_graph->sorted_nodes, p_graph->n_nodes,
		 sizeof (struct node *), cmp_nodes1_code);
      }
    else
      {
	  /* Nodes are identified by an INTEGER id */
	  qsort (p_graph->sorted_nodes, p_graph->n_nodes,
		 sizeof (struct node *), cmp_nodes1_id);
      }
}

static void
insert_node (struct graph *p_graph, sqlite3_int64 id, const char *code,
	     int node_code)
{
/* inserts a Node into the preliminary list */
    struct pre_node *pP = malloc (sizeof (struct pre_node));
    if (node_code)
      {
	  /* Node is identified by a TEXT code */
	  int len = strlen (code);
	  if (len > 31)
	    {
		memcpy (pP->code, code, 31);
		*(pP->code + 31) = '\0';
	    }
	  else
	      strcpy (pP->code, code);
	  pP->id = -1;
      }
    else
      {
	  /* Nodes are identified by an INTEGER id */
	  *(pP->code) = '\0';
	  pP->id = id;
      }
    pP->next = NULL;
    if (!(p_graph->first_pre))
	p_graph->first_pre = pP;
    if (p_graph->last_pre)
	p_graph->last_pre->next = pP;
    p_graph->last_pre = pP;
}

static void
add_node (struct graph *p_graph, sqlite3_int64 id, const char *code)
{
/* inserts a Node into the final list */
    int len;
    struct node *pN = malloc (sizeof (struct node));
    if (p_graph->node_code)
      {
	  /* Nodes are identified by a TEXT code */
	  len = strlen (code) + 1;
	  if (len > p_graph->max_code_length)
	      p_graph->max_code_length = len;
	  strcpy (pN->code, code);
	  pN->id = -1;
      }
    else
      {
	  /* Nodes are identified by an INTEGER id */
	  *(pN->code) = '\0';
	  pN->id = id;
      }
    pN->x = DBL_MAX;
    pN->y = DBL_MAX;
    pN->first_incoming = NULL;
    pN->last_incoming = NULL;
    pN->first_outcoming = NULL;
    pN->last_outcoming = NULL;
    pN->next = NULL;
    if (!(p_graph->first_node))
	p_graph->first_node = pN;
    if (p_graph->last_node)
	p_graph->last_node->next = pN;
    p_graph->last_node = pN;
}

static struct node *
process_node (struct graph *p_graph, sqlite3_int64 id, const char *code,
	      double x, double y, struct node **pOther)
{
/* inserts a new node or retrieves an already defined one */
    struct node *pN = find_node (p_graph, id, code);
    *pOther = NULL;
    if (pN)
      {
	  /* this Node already exists into the sorted list */
	  if (pN->x == DBL_MAX && pN->y == DBL_MAX)
	    {
		pN->x = x;
		pN->y = y;
	    }
	  else
	    {
		if (pN->x == x && pN->y == y)
		    ;
		else
		    *pOther = pN;
	    }
	  return pN;
      }
/* unexpected error; undefined Node */
    return NULL;
}

static void
add_incoming_arc (struct node *pN, struct arc *pA)
{
/* adds an incoming Arc to a Node */
    struct arc_ref *pAR = malloc (sizeof (struct arc_ref));
    pAR->reference = pA;
    pAR->next = NULL;
    if (!(pN->first_incoming))
	pN->first_incoming = pAR;
    if (pN->last_incoming)
	pN->last_incoming->next = pAR;
    pN->last_incoming = pAR;
}

static void
add_outcoming_arc (struct node *pN, struct arc *pA)
{
/* adds an outcoming Arc to a Node */
    struct arc_ref *pAR = malloc (sizeof (struct arc_ref));
    pAR->reference = pA;
    pAR->next = NULL;
    if (!(pN->first_outcoming))
	pN->first_outcoming = pAR;
    if (pN->last_outcoming)
	pN->last_outcoming->next = pAR;
    pN->last_outcoming = pAR;
}

static void
add_arc (struct graph *p_graph, sqlite3_int64 rowid, sqlite3_int64 id_from,
	 sqlite3_int64 id_to, const char *code_from, const char *code_to,
	 double node_from_x, double node_from_y, double node_to_x,
	 double node_to_y, double cost)
{
/* inserting an arc into the memory structures */
    struct node *pFrom;
    struct node *pTo;
    struct node *pN2;
    struct arc *pA;
    char xRowid[128];
    sprintf (xRowid, FORMAT_64, rowid);
    pFrom =
	process_node (p_graph, id_from, code_from, node_from_x, node_from_y,
		      &pN2);
    if (pN2)
      {
	  printf ("ERROR: arc ROWID=%s; nodeFrom coord inconsistency\n",
		  xRowid);
	  printf ("\twas: x=%1.6f y=%1.6f\n", pN2->x, pN2->y);
	  printf ("\tnow: x=%1.6f y=%1.6f\n", node_from_x, node_from_y);
	  p_graph->error = 1;
      }
    pTo = process_node (p_graph, id_to, code_to, node_to_x, node_to_y, &pN2);
    if (pN2)
      {
	  printf ("ERROR: arc ROWID=%s; nodeTo coord inconsistency\n", xRowid);
	  printf ("\twas: x=%1.6f y=%1.6f\n", pN2->x, pN2->y);
	  printf ("\tnow: x=%1.6f y=%1.6f\n", node_to_x, node_to_y);
	  p_graph->error = 1;
      }
    if (!pFrom)
      {
	  printf ("ERROR: arc ROWID=%s internal error: missing NodeFrom\n",
		  xRowid);
	  p_graph->error = 1;
      }
    if (!pTo)
      {
	  printf ("ERROR: arc ROWID=%s internal error: missing NodeTo\n",
		  xRowid);
	  p_graph->error = 1;
      }
    if (pFrom == pTo)
      {
	  printf ("ERROR: arc ROWID=%s is a closed ring\n", xRowid);
	  p_graph->error = 1;
      }
    if (p_graph->error)
	return;
    pA = malloc (sizeof (struct arc));
    pA->rowid = rowid;
    pA->from = pFrom;
    pA->to = pTo;
    pA->cost = cost;
    pA->next = NULL;
    if (!(p_graph->first_arc))
	p_graph->first_arc = pA;
    if (p_graph->last_arc)
	p_graph->last_arc->next = pA;
    p_graph->last_arc = pA;
/* updating Node connections */
    add_outcoming_arc (pFrom, pA);
    add_incoming_arc (pTo, pA);
}

static int
cmp_prenodes_code (const void *p1, const void *p2)
{
/* compares two preliminary nodes  by CODE [for QSORT] */
    struct pre_node *pP1 = *((struct pre_node **) p1);
    struct pre_node *pP2 = *((struct pre_node **) p2);
    return strcmp (pP1->code, pP2->code);
}

static int
cmp_prenodes_id (const void *p1, const void *p2)
{
/* compares two preliminary nodes  by ID [for QSORT] */
    struct pre_node *pP1 = *((struct pre_node **) p1);
    struct pre_node *pP2 = *((struct pre_node **) p2);
    if (pP1->id == pP2->id)
	return 0;
    if (pP1->id > pP2->id)
	return 1;
    return -1;
}

static void
init_nodes (struct graph *p_graph)
{
/* prepares the final Nodes list */
    sqlite3_int64 last_id;
    char last_code[32];
    int i;
    struct pre_node *pP;
    p_graph->n_pre_nodes = 0;
/* sorting preliminary nodes */
    if (p_graph->sorted_pre_nodes)
      {
	  /* we must free the already existent sorted list */
	  free (p_graph->sorted_pre_nodes);
      }
    p_graph->sorted_pre_nodes = NULL;
    pP = p_graph->first_pre;
    while (pP)
      {
	  (p_graph->n_pre_nodes)++;
	  pP = pP->next;
      }
    if (!(p_graph->n_pre_nodes))
	return;
    p_graph->sorted_pre_nodes =
	malloc (sizeof (struct pre_node *) * p_graph->n_pre_nodes);
    i = 0;
    pP = p_graph->first_pre;
    while (pP)
      {
	  *(p_graph->sorted_pre_nodes + i++) = pP;
	  pP = pP->next;
      }
    if (p_graph->node_code)
      {
	  /* Nodes are identified by a TEXT code */
	  qsort (p_graph->sorted_pre_nodes, p_graph->n_pre_nodes,
		 sizeof (struct pre_node *), cmp_prenodes_code);
      }
    else
      {
	  /* Nodes are identified by an INTEGER id */
	  qsort (p_graph->sorted_pre_nodes, p_graph->n_pre_nodes,
		 sizeof (struct pre_node *), cmp_prenodes_id);
      }
/* creating the final Nodes linked list */
    last_id = -1;
    *last_code = '\0';
    for (i = 0; i < p_graph->n_pre_nodes; i++)
      {
	  pP = *(p_graph->sorted_pre_nodes + i);
	  if (p_graph->node_code)
	    {
		/* Nodes are identified by a TEXT code */
		if (strcmp (pP->code, last_code) != 0)
		    add_node (p_graph, pP->id, pP->code);
	    }
	  else
	    {
		/* Nodes are identified by an INTEGER id */
		if (pP->id != last_id)
		    add_node (p_graph, pP->id, pP->code);
	    }
	  last_id = pP->id;
	  strcpy (last_code, pP->code);
      }
/* sorting the final Nodes list */
    sort_nodes (p_graph);
/* cleaning up the preliminary Nodes structs */
    graph_free_pre (p_graph);
}

static void
print_report (struct graph *p_graph)
{
/* printing the final report */
    int cnt = 0;
    int max_in = 0;
    int max_out = 0;
    int card_in;
    int card_out;
    int card_1 = 0;
    int card_2 = 0;
    struct node *pN;
    struct arc_ref *pAR;
    struct arc *pA = p_graph->first_arc;
    while (pA)
      {
	  /* counting the Arcs */
	  cnt++;
	  pA = pA->next;
      }
    pN = p_graph->first_node;
    while (pN)
      {
	  card_in = 0;
	  pAR = pN->first_incoming;
	  while (pAR)
	    {
		card_in++;
		pAR = pAR->next;
	    }
	  card_out = 0;
	  pAR = pN->first_outcoming;
	  while (pAR)
	    {
		card_out++;
		pAR = pAR->next;
	    }
	  if (card_in > max_in)
	      max_in = card_in;
	  if (card_out > max_out)
	      max_out = card_out;
	  if (card_in == 1 && card_out == 1)
	      card_1++;
	  if (card_in == 2 && card_out == 2)
	      card_2++;
	  pN = pN->next;
      }
    printf ("\nStatistics\n");
    printf
	("==================================================================\n");
    printf ("\t# Arcs : %d\n", cnt);
    printf ("\t# Nodes: %d\n", p_graph->n_nodes);
    printf ("\tNode max  incoming arcs: %d\n", max_in);
    printf ("\tNode max outcoming arcs: %d\n", max_out);
    printf ("\t# Nodes   cardinality=1: %d [terminal nodes]\n", card_1);
    printf ("\t# Nodes   cardinality=2: %d [meaningless, pass-through]\n",
	    card_2);
    printf
	("==================================================================\n");
}

static struct arc **
prepareOutcomings (struct node *pN, int *count)
{
/* preparing the outcoming arc array */
    struct arc **arc_array;
    int n = 0;
    int i;
    int ok;
    struct arc_ref *pAR;
    struct arc *pA0;
    struct arc *pA1;
    pAR = pN->first_outcoming;
    while (pAR)
      {
	  /* counting how many outcoming arsc are there */
	  n++;
	  pAR = pAR->next;
      }
    if (!n)
      {
	  *count = 0;
	  return NULL;
      }
    arc_array = malloc (sizeof (struct arc *) * n);
    i = 0;
    pAR = pN->first_outcoming;
    while (pAR)
      {
	  /* populating the arcs array */
	  *(arc_array + i++) = pAR->reference;
	  pAR = pAR->next;
      }
    ok = 1;
    while (ok)
      {
	  /* bubble sorting the arcs by Cost */
	  ok = 0;
	  for (i = 1; i < n; i++)
	    {
		pA0 = *(arc_array + i - 1);
		pA1 = *(arc_array + i);
		if (pA0->cost > pA1->cost)
		  {
		      /* swapping the arcs */
		      *(arc_array + i - 1) = pA1;
		      *(arc_array + i) = pA0;
		      ok = 1;
		  }
	    }
      }
    *count = n;
    return arc_array;
}

static void
output_node (unsigned char *auxbuf, int *size, int ind, int node_code,
	     int max_node_length, struct node *pN, int endian_arch,
	     int a_star_supported)
{
/* exporting a Node into NETWORK-DATA */
    int n_star;
    int i;
    struct arc **arc_array;
    struct arc *pA;
    unsigned char *out = auxbuf;
    *out++ = GAIA_NET_NODE;
    gaiaExport32 (out, ind, 1, endian_arch);	/* the Node internal index */
    out += 4;
    if (node_code)
      {
	  /* Nodes are identified by a TEXT Code */
	  memset (out, '\0', max_node_length);
	  strcpy ((char *) out, pN->code);
	  out += max_node_length;
      }
    else
      {
	  /* Nodes are identified by an INTEGER Id */
	  gaiaExportI64 (out, pN->id, 1, endian_arch);	/* the Node ID */
	  out += 8;
      }
    if (a_star_supported)
      {
	  /* in order to support the A* algorithm [X,Y] are required for each node */
	  gaiaExport64 (out, pN->x, 1, endian_arch);
	  out += 8;
	  gaiaExport64 (out, pN->y, 1, endian_arch);
	  out += 8;
      }
    arc_array = prepareOutcomings (pN, &n_star);
    gaiaExport16 (out, n_star, 1, endian_arch);	/* # of outcoming arcs */
    out += 2;
    for (i = 0; i < n_star; i++)
      {
	  /* exporting the outcoming arcs */
	  pA = *(arc_array + i);
	  *out++ = GAIA_NET_ARC;
	  gaiaExportI64 (out, pA->rowid, 1, endian_arch);	/* the Arc rowid */
	  out += 8;
	  gaiaExport32 (out, pA->to->internal_index, 1, endian_arch);	/* the ToNode internal index */
	  out += 4;
	  gaiaExport64 (out, pA->cost, 1, endian_arch);	/* the Arc Cost */
	  out += 8;
	  *out++ = GAIA_NET_END;
      }
    if (arc_array)
	free (arc_array);
    *out++ = GAIA_NET_END;
    *size = out - auxbuf;
}

static int
create_network_data (sqlite3 * handle, const char *out_table,
		     int force_creation, struct graph *p_graph,
		     const char *table, const char *from_column,
		     const char *to_column, const char *geom_column,
		     const char *name_column, int a_star_supported,
		     double a_star_coeff)
{
/* creates the NETWORK-DATA table */
    int ret;
    char sql[1024];
    char *err_msg = NULL;
    unsigned char *auxbuf = malloc (MAX_BLOCK);
    unsigned char *buf = malloc (MAX_BLOCK);
    unsigned char *out;
    sqlite3_stmt *stmt;
    int i;
    int size;
    int endian_arch = gaiaEndianArch ();
    struct node *pN;
    int pk = 0;
    int nodes_cnt;
    int len;
    for (i = 0; i < p_graph->n_nodes; i++)
      {
	  /* setting the internal index to each Node */
	  pN = *(p_graph->sorted_nodes + i);
	  pN->internal_index = i;
      }
/* starts a transaction */
    strcpy (sql, "BEGIN");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("BEGIN error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (force_creation)
      {
	  sprintf (sql, "DROP TABLE IF EXISTS \"%s\"", out_table);
	  ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
	  if (ret != SQLITE_OK)
	    {
		printf ("DROP TABLE error: %s\n", err_msg);
		sqlite3_free (err_msg);
		goto abort;
	    }
      }
/* creating the NETWORK-DATA table */
    sprintf (sql, "CREATE TABLE \"%s\" (", out_table);
    strcat (sql, "\"Id\" INTEGER PRIMARY KEY, \"NetworkData\" BLOB NOT NULL)");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("CREATE TABLE error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
/* preparing the SQL statement */
    sprintf (sql, "INSERT INTO \"%s\" (\"Id\", \"NetworkData\") VALUES (?, ?)",
	     out_table);
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  printf ("INSERT error: %s\n", sqlite3_errmsg (handle));
	  goto abort;
      }
    if (pk == 0)
      {
	  /* preparing the HEADER block */
	  out = buf;
	  if (a_star_supported)
	      *out++ = GAIA_NET64_A_STAR_START;
	  else
	      *out++ = GAIA_NET64_START;
	  *out++ = GAIA_NET_HEADER;
	  gaiaExport32 (out, p_graph->n_nodes, 1, endian_arch);	/* how many Nodes are there */
	  out += 4;
	  if (p_graph->node_code)
	      *out++ = GAIA_NET_CODE;	/* Nodes are identified by a TEXT code */
	  else
	      *out++ = GAIA_NET_ID;	/* Nodes are identified by an INTEGER id */
	  if (p_graph->node_code)
	      *out++ = p_graph->max_code_length;	/* max TEXT code length */
	  else
	      *out++ = 0x00;
	  /* inserting the main Table name */
	  *out++ = GAIA_NET_TABLE;
	  len = strlen (table) + 1;
	  gaiaExport16 (out, len, 1, endian_arch);	/* the Table Name length, including last '\0' */
	  out += 2;
	  memset (out, '\0', len);
	  strcpy ((char *) out, table);
	  out += len;
	  /* inserting the NodeFrom column name */
	  *out++ = GAIA_NET_FROM;
	  len = strlen (from_column) + 1;
	  gaiaExport16 (out, len, 1, endian_arch);	/* the NodeFrom column Name length, including last '\0' */
	  out += 2;
	  memset (out, '\0', len);
	  strcpy ((char *) out, from_column);
	  out += len;
	  /* inserting the NodeTo column name */
	  *out++ = GAIA_NET_TO;
	  len = strlen (to_column) + 1;
	  gaiaExport16 (out, len, 1, endian_arch);	/* the NodeTo column Name length, including last '\0' */
	  out += 2;
	  memset (out, '\0', len);
	  strcpy ((char *) out, to_column);
	  out += len;
	  /* inserting the Geometry column name */
	  *out++ = GAIA_NET_GEOM;
	  if (!geom_column)
	      len = 1;
	  else
	      len = strlen (geom_column) + 1;
	  gaiaExport16 (out, len, 1, endian_arch);	/* the Geometry column Name length, including last '\0' */
	  out += 2;
	  memset (out, '\0', len);
	  if (geom_column)
	      strcpy ((char *) out, geom_column);
	  out += len;
	  /* inserting the Name column name - may be empty */
	  *out++ = GAIA_NET_NAME;
	  if (!name_column)
	      len = 1;
	  else
	      len = strlen (name_column) + 1;
	  gaiaExport16 (out, len, 1, endian_arch);	/* the Name column Name length, including last '\0' */
	  out += 2;
	  memset (out, '\0', len);
	  if (name_column)
	      strcpy ((char *) out, name_column);
	  out += len;
	  if (a_star_supported)
	    {
		/* inserting the A* Heuristic Coeff */
		*out++ = GAIA_NET_A_STAR_COEFF;
		gaiaExport64 (out, a_star_coeff, 1, endian_arch);
		out += 8;
	    }
	  *out++ = GAIA_NET_END;
	  /* INSERTing the Header block */
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_int64 (stmt, 1, pk);
	  sqlite3_bind_blob (stmt, 2, buf, out - buf, SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		sqlite3_finalize (stmt);
		goto abort;
	    }
	  pk++;
	  /* preparing a new block */
	  out = buf;
	  *out++ = GAIA_NET_BLOCK;
	  gaiaExport16 (out, 0, 1, endian_arch);	/* how many Nodes are into this block */
	  out += 2;
	  nodes_cnt = 0;
      }
    for (i = 0; i < p_graph->n_nodes; i++)
      {
	  /* looping on each Node */
	  pN = *(p_graph->sorted_nodes + i);
	  output_node (auxbuf, &size, i, p_graph->node_code,
		       p_graph->max_code_length, pN, endian_arch,
		       a_star_supported);
	  if (size >= (MAX_BLOCK - (out - buf)))
	    {
		/* inserting the last block */
		gaiaExport16 (buf + 1, nodes_cnt, 1, endian_arch);	/* how many Nodes are into this block */
		sqlite3_reset (stmt);
		sqlite3_clear_bindings (stmt);
		sqlite3_bind_int64 (stmt, 1, pk);
		sqlite3_bind_blob (stmt, 2, buf, out - buf, SQLITE_STATIC);
		ret = sqlite3_step (stmt);
		if (ret == SQLITE_DONE || ret == SQLITE_ROW)
		    ;
		else
		  {
		      printf ("sqlite3_step() error: %s\n",
			      sqlite3_errmsg (handle));
		      sqlite3_finalize (stmt);
		      goto abort;
		  }
		pk++;
		/* preparing a new block */
		out = buf;
		*out++ = GAIA_NET_BLOCK;
		gaiaExport16 (out, 0, 1, endian_arch);	/* how many Nodes are into this block */
		out += 2;
		nodes_cnt = 0;
	    }
	  /* inserting the current Node into the block */
	  nodes_cnt++;
	  memcpy (out, auxbuf, size);
	  out += size;
      }
    if (nodes_cnt)
      {
	  /* inserting the last block */
	  gaiaExport16 (buf + 1, nodes_cnt, 1, endian_arch);	/* how many Nodes are into this block */
	  sqlite3_reset (stmt);
	  sqlite3_clear_bindings (stmt);
	  sqlite3_bind_int64 (stmt, 1, pk);
	  sqlite3_bind_blob (stmt, 2, buf, out - buf, SQLITE_STATIC);
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	      ;
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		sqlite3_finalize (stmt);
		goto abort;
	    }
      }
    sqlite3_finalize (stmt);
/* commits the transaction */
    strcpy (sql, "COMMIT");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("COMMIT error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (buf)
	free (buf);
    if (auxbuf)
	free (auxbuf);
    return 1;
  abort:
    if (buf)
	free (buf);
    if (auxbuf)
	free (auxbuf);
    return 0;
}

static int
create_virtual_network (sqlite3 * handle, const char *out_table,
			const char *virt_table, int force_creation)
{
/* creates the VirtualNetwork table */
    int ret;
    char sql[1024];
    char *err_msg = NULL;
    if (force_creation)
      {
	  sprintf (sql, "DROP TABLE IF EXISTS \"%s\"", virt_table);
	  ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
	  if (ret != SQLITE_OK)
	    {
		printf ("DROP TABLE error: %s\n", err_msg);
		sqlite3_free (err_msg);
		goto abort;
	    }
      }
/* creating the VirtualNetwork table */
    sprintf (sql, "CREATE VIRTUAL TABLE \"%s\" USING  VirtualNetwork(\"%s\")",
	     virt_table, out_table);
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("CREATE TABLE error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    return 1;
  abort:
    return 0;
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
validate (const char *path, const char *table, const char *from_column,
	  const char *to_column, const char *cost_column,
	  const char *geom_column, const char *name_column,
	  const char *oneway_tofrom, const char *oneway_fromto,
	  int bidirectional, const char *out_table, const char *virt_table,
	  int force_creation, int a_star_supported)
{
/* performs all the actual network validation */
    int ret;
    sqlite3 *handle;
    sqlite3_stmt *stmt;
    struct graph *p_graph = NULL;
    char sql[1024];
    char sql2[128];
    char **results;
    int n_rows;
    int n_columns;
    int i;
    char *err_msg = NULL;
    char *col_name;
    int type;
    int ok_from_column = 0;
    int ok_to_column = 0;
    int ok_cost_column = 0;
    int ok_geom_column = 0;
    int ok_name_column = 0;
    int ok_oneway_tofrom = 0;
    int ok_oneway_fromto = 0;
    int from_null = 0;
    int from_int = 0;
    int from_double = 0;
    int from_text = 0;
    int from_blob = 0;
    int to_null = 0;
    int to_int = 0;
    int to_double = 0;
    int to_text = 0;
    int to_blob = 0;
    int cost_null = 0;
    int cost_text = 0;
    int cost_blob = 0;
    int tofrom_null = 0;
    int tofrom_double = 0;
    int tofrom_text = 0;
    int tofrom_blob = 0;
    int fromto_null = 0;
    int fromto_double = 0;
    int fromto_text = 0;
    int fromto_blob = 0;
    int geom_null = 0;
    int geom_not_linestring = 0;
    int col_n;
    int fromto_n;
    int tofrom_n;
    sqlite3_int64 rowid;
    sqlite3_int64 id_from;
    sqlite3_int64 id_to;
    char code_from[1024];
    char code_to[1024];
    double node_from_x;
    double node_from_y;
    double node_to_x;
    double node_to_y;
    double cost;
    int fromto;
    int tofrom;
    char xRowid[128];
    char xIdFrom[128];
    char xIdTo[128];
    int aStarLength;
    double a_star_length;
    double a_star_coeff;
    double min_a_star_coeff = DBL_MAX;
    void *cache;

/* showing the SQLite version */
    fprintf (stderr, "SQLite version: %s\n", sqlite3_libversion ());
/* showing the SpatiaLite version */
    fprintf (stderr, "SpatiaLite version: %s\n", spatialite_version ());
/* trying to connect the SpatiaLite DB  */
    ret =
	sqlite3_open_v2 (path, &handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", path,
		   sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  return;
      }
    cache = spatialite_alloc_connection ();
    spatialite_init_ex (handle, cache, 0);
    spatialite_autocreate (handle);

    fprintf (stderr, "Step   I - checking for table and columns existence\n");
/* reporting args */
    printf ("\nspatialite-network\n\n");
    printf
	("==================================================================\n");
    printf ("   SpatiaLite db: %s\n", path);
    printf ("validating table: %s\n\n", table);
    printf ("columns layout\n");
    printf
	("==================================================================\n");
    printf ("FromNode: %s\n", from_column);
    printf ("  ToNode: %s\n", to_column);
    if (!cost_column)
	printf ("    Cost: GLength(%s)\n", geom_column);
    else
	printf ("    Cost: %s\n", cost_column);
    if (!name_column)
	printf ("    Name: *unused*\n");
    else
	printf ("    Name: %s\n", name_column);
    printf ("Geometry: %s\n\n", geom_column);
    if (bidirectional)
      {
	  printf ("assuming arcs to be BIDIRECTIONAL\n");
	  if (oneway_tofrom && oneway_fromto)
	    {
		printf ("OneWay To->From: %s\n", oneway_tofrom);
		printf ("OneWay From->To: %s\n", oneway_fromto);
	    }
      }
    else
	printf ("assuming arcs to be UNIDIRECTIONAL\n");
    if (out_table)
      {
	  printf ("\nNETWORK-DATA table creation required: '%s'\n", out_table);
	  if (virt_table)
	      printf ("\nVirtualNetwork table creation required: '%s'\n",
		      virt_table);
	  if (force_creation)
	      printf ("Overwrite allowed if table already exists\n");
	  else
	      printf ("Overwrite not allowed if table already exists\n");
      }
    else
	printf
	    ("\nsimple validation required\n[NETWORK-DATA table creation is disabled]\n");
    printf
	("==================================================================\n\n");
/* checking for table existence */
    sprintf (sql,
	     "SELECT \"tbl_name\" FROM \"sqlite_master\" WHERE Upper(\"tbl_name\") = Upper('%s') and \"type\" = 'table'",
	     table);
    ret =
	sqlite3_get_table (handle, sql, &results, &n_rows, &n_columns,
			   &err_msg);
    if (ret != SQLITE_OK)
      {
/* some error occurred */
	  fprintf (stderr, "query#1 SQL error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (n_rows == 0)
      {
	  /* required table does not exists */
	  printf ("ERROR: table '%s' does not exists\n", table);
	  goto abort;
      }
    else
	sqlite3_free_table (results);
/* checking for columns existence */
    sprintf (sql, "PRAGMA table_info(\"%s\")", table);
    ret =
	sqlite3_get_table (handle, sql, &results, &n_rows, &n_columns,
			   &err_msg);
    if (ret != SQLITE_OK)
      {
/* some error occurred */
	  fprintf (stderr, "query#2 SQL error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (n_rows > 1)
      {
	  for (i = 1; i <= n_rows; i++)
	    {
		col_name = results[(i * n_columns) + 1];
		if (strcasecmp (from_column, col_name) == 0)
		    ok_from_column = 1;
		if (strcasecmp (to_column, col_name) == 0)
		    ok_to_column = 1;
		if (cost_column)
		  {
		      if (strcasecmp (cost_column, col_name) == 0)
			  ok_cost_column = 1;
		  }
		if (strcasecmp (geom_column, col_name) == 0)
		    ok_geom_column = 1;
		if (name_column)
		  {
		      if (strcasecmp (name_column, col_name) == 0)
			  ok_name_column = 1;
		  }
		if (oneway_tofrom)
		  {
		      if (strcasecmp (oneway_tofrom, col_name) == 0)
			  ok_oneway_tofrom = 1;
		  }
		if (oneway_fromto)
		  {
		      if (strcasecmp (oneway_fromto, col_name) == 0)
			  ok_oneway_fromto = 1;
		  }
	    }
	  sqlite3_free_table (results);
      }
    if (!ok_from_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		from_column);
    if (!ok_to_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		to_column);
    if (cost_column && !ok_cost_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		cost_column);
    if (!ok_geom_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		geom_column);
    if (name_column && !ok_name_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		name_column);
    if (oneway_tofrom && !ok_oneway_tofrom)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		oneway_tofrom);
    if (oneway_fromto && !ok_oneway_fromto)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		oneway_fromto);
    if (!name_column)
	ok_name_column = 1;
    if (ok_from_column && ok_to_column && ok_geom_column && ok_name_column)
	;
    else
	goto abort;
    if (cost_column && !ok_cost_column)
	goto abort;
    if (oneway_tofrom && !ok_oneway_tofrom)
	goto abort;
    if (oneway_fromto && !ok_oneway_fromto)
	goto abort;
    fprintf (stderr, "Step  II - checking value types consistency\n");
/* checking column types */
    p_graph = graph_init ();
    sprintf (sql, "SELECT \"%s\", \"%s\", GeometryType(\"%s\")",
	     from_column, to_column, geom_column);
    col_n = 3;
    if (cost_column)
      {
	  sprintf (sql2, ", \"%s\"", cost_column);
	  strcat (sql, sql2);
	  col_n++;
      }
    if (oneway_tofrom)
      {
	  sprintf (sql2, ", \"%s\"", oneway_tofrom);
	  strcat (sql, sql2);
	  tofrom_n = col_n;
	  col_n++;
      }
    if (oneway_fromto)
      {
	  sprintf (sql2, ", \"%s\"", oneway_fromto);
	  strcat (sql, sql2);
	  fromto_n = col_n;
	  col_n++;
      }
    sprintf (sql2, " FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  printf ("query#4 SQL error: %s\n", sqlite3_errmsg (handle));
	  goto abort;
      }
    n_columns = sqlite3_column_count (stmt);
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		/* the NodeFrom type */
		type = sqlite3_column_type (stmt, 0);
		if (type == SQLITE_NULL)
		    from_null = 1;
		if (type == SQLITE_INTEGER)
		  {
		      from_int = 1;
		      id_from = sqlite3_column_int64 (stmt, 0);
		      insert_node (p_graph, id_from, "", 0);
		  }
		if (type == SQLITE_FLOAT)
		    from_double = 1;
		if (type == SQLITE_TEXT)
		  {
		      from_text = 1;
		      strcpy (code_from,
			      (char *) sqlite3_column_text (stmt, 0));
		      insert_node (p_graph, -1, code_from, 1);
		  }
		if (type == SQLITE_BLOB)
		    from_blob = 1;
		/* the NodeTo type */
		type = sqlite3_column_type (stmt, 1);
		if (type == SQLITE_NULL)
		    to_null = 1;
		if (type == SQLITE_INTEGER)
		  {
		      to_int = 1;
		      id_to = sqlite3_column_int64 (stmt, 1);
		      insert_node (p_graph, id_to, "", 0);
		  }
		if (type == SQLITE_FLOAT)
		    to_double = 1;
		if (type == SQLITE_TEXT)
		  {
		      to_text = 1;
		      strcpy (code_to, (char *) sqlite3_column_text (stmt, 1));
		      insert_node (p_graph, -1, code_to, 1);
		  }
		if (type == SQLITE_BLOB)
		    to_blob = 1;
		/* the Geometry type */
		type = sqlite3_column_type (stmt, 2);
		if (type == SQLITE_NULL)
		    geom_null = 1;
		else if (strcmp
			 ("LINESTRING",
			  (char *) sqlite3_column_text (stmt, 2)) != 0)
		    geom_not_linestring = 1;
		col_n = 3;
		if (cost_column)
		  {
		      /* the Cost type */
		      type = sqlite3_column_type (stmt, col_n);
		      col_n++;
		      if (type == SQLITE_NULL)
			  cost_null = 1;
		      if (type == SQLITE_TEXT)
			  cost_text = 1;
		      if (type == SQLITE_BLOB)
			  cost_blob = 1;
		  }
		if (oneway_fromto)
		  {
		      /* the FromTo type */
		      type = sqlite3_column_type (stmt, col_n);
		      col_n++;
		      if (type == SQLITE_NULL)
			  fromto_null = 1;
		      if (type == SQLITE_FLOAT)
			  fromto_double = 1;
		      if (type == SQLITE_TEXT)
			  fromto_text = 1;
		      if (type == SQLITE_BLOB)
			  fromto_blob = 1;
		  }
		if (oneway_tofrom)
		  {
		      /* the ToFrom type */
		      type = sqlite3_column_type (stmt, col_n);
		      col_n++;
		      if (type == SQLITE_NULL)
			  tofrom_null = 1;
		      if (type == SQLITE_FLOAT)
			  tofrom_double = 1;
		      if (type == SQLITE_TEXT)
			  tofrom_text = 1;
		      if (type == SQLITE_BLOB)
			  tofrom_blob = 1;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		sqlite3_finalize (stmt);
		goto abort;
	    }
      }
    sqlite3_finalize (stmt);
    ret = 1;
    if (from_null)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n", table,
		  from_column);
	  ret = 0;
      }
    if (from_blob)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n", table,
		  from_column);
	  ret = 0;
      }
    if (from_double)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n", table,
		  from_column);
	  ret = 0;
      }
    if (to_null)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n", table,
		  to_column);
	  ret = 0;
      }
    if (to_blob)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n", table,
		  to_column);
	  ret = 0;
      }
    if (to_double)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n", table,
		  to_column);
	  ret = 0;
      }
    if (geom_null)
      {
	  printf
	      ("ERROR: column \"%s\".\"%s\" contains NULL values [or invalid Geometries]\n",
	       table, geom_column);
	  ret = 0;
      }
    if (geom_not_linestring)
      {
	  printf
	      ("ERROR: column \"%s\".\"%s\" contains Geometries not of LINESTRING type\n",
	       table, geom_column);
	  ret = 0;
      }
    if (cost_column)
      {
	  if (cost_null)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n",
			table, cost_column);
		ret = 0;
	    }
	  if (cost_blob)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n",
			table, cost_column);
		ret = 0;
	    }
	  if (cost_text)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains TEXT values\n",
			table, cost_column);
		ret = 0;
	    }
      }
    if (oneway_fromto)
      {
	  if (fromto_null)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n",
			table, oneway_fromto);
		ret = 0;
	    }
	  if (fromto_blob)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n",
			table, oneway_fromto);
		ret = 0;
	    }
	  if (fromto_text)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains TEXT values\n",
			table, oneway_fromto);
		ret = 0;
	    }
	  if (fromto_double)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n",
			table, oneway_fromto);
		ret = 0;
	    }
      }
    if (oneway_tofrom)
      {
	  if (tofrom_null)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
	  if (tofrom_blob)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
	  if (tofrom_text)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains TEXT values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
	  if (tofrom_double)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
      }
    if (!ret)
	goto abort;
    if (from_int && to_int)
      {
	  /* each node is identified by an INTEGER id */
	  p_graph->node_code = 0;
      }
    else if (from_text && to_text)
      {
	  /* each node is identified by a TEXT code */
	  p_graph->node_code = 1;
      }
    else
      {
	  printf ("ERROR: NodeFrom / NodeTo have different value types\n");
	  goto abort;
      }
    init_nodes (p_graph);
    fprintf (stderr, "Step III - checking topological consistency\n");
/* checking topological consistency */
    sprintf (sql,
	     "SELECT ROWID, \"%s\", \"%s\", X(StartPoint(\"%s\")), Y(StartPoint(\"%s\")), X(EndPoint(\"%s\")), Y(EndPoint(\"%s\"))",
	     from_column, to_column, geom_column, geom_column, geom_column,
	     geom_column);
    if (a_star_supported)
      {
	  /* supporting A* algorithm */
	  if (cost_column)
	    {
		sprintf (sql2, ", \"%s\", GLength(\"%s\")", cost_column,
			 geom_column);
		strcat (sql, sql2);
		col_n = 9;
		aStarLength = 1;
	    }
	  else
	    {
		sprintf (sql2, ", GLength(\"%s\")", geom_column);
		strcat (sql, sql2);
		col_n = 8;
		aStarLength = 0;
		min_a_star_coeff = 1.0;
	    }
      }
    else
      {
	  /* A* algorithm unsupported */
	  if (cost_column)
	    {
		sprintf (sql2, ", \"%s\"", cost_column);
		strcat (sql, sql2);
	    }
	  else
	    {
		sprintf (sql2, ", GLength(\"%s\")", geom_column);
		strcat (sql, sql2);
	    }
	  col_n = 8;
      }
    if (oneway_tofrom)
      {
	  sprintf (sql2, ", \"%s\"", oneway_tofrom);
	  strcat (sql, sql2);
	  tofrom_n = col_n;
	  col_n++;
      }
    if (oneway_fromto)
      {
	  sprintf (sql2, ", \"%s\"", oneway_fromto);
	  strcat (sql, sql2);
	  fromto_n = col_n;
	  col_n++;
      }
    sprintf (sql2, " FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  printf ("query#4 SQL error: %s\n", sqlite3_errmsg (handle));
	  goto abort;
      }
    n_columns = sqlite3_column_count (stmt);
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		fromto = 1;
		tofrom = 1;
		if (p_graph->node_code)
		  {
		      id_from = -1;
		      id_to = -1;
		  }
		else
		  {
		      *code_from = '\0';
		      *code_to = '\0';
		  }
		/* fetching the ROWID */
		rowid = sqlite3_column_int64 (stmt, 0);
		/* fetching the NodeFrom value */
		if (p_graph->node_code)
		    strcpy (code_from, (char *) sqlite3_column_text (stmt, 1));
		else
		    id_from = sqlite3_column_int64 (stmt, 1);
		/* fetching the NodeTo value */
		if (p_graph->node_code)
		    strcpy (code_to, (char *) sqlite3_column_text (stmt, 2));
		else
		    id_to = sqlite3_column_int64 (stmt, 2);
		/* fetching the NodeFromX value */
		node_from_x = sqlite3_column_double (stmt, 3);
		/* fetching the NodeFromY value */
		node_from_y = sqlite3_column_double (stmt, 4);
		/* fetching the NodeFromX value */
		node_to_x = sqlite3_column_double (stmt, 5);
		/* fetching the NodeFromY value */
		node_to_y = sqlite3_column_double (stmt, 6);
		/* fetching the Cost value */
		cost = sqlite3_column_double (stmt, 7);
		if (aStarLength)
		  {
		      /* supporting A* - fetching the arc length */
		      a_star_length = sqlite3_column_double (stmt, 8);
		      a_star_coeff = cost / a_star_length;
		      if (a_star_coeff < min_a_star_coeff)
			  min_a_star_coeff = a_star_coeff;
		  }
		if (oneway_fromto)
		  {
		      /* fetching the OneWay-FromTo value */
		      fromto = sqlite3_column_int (stmt, fromto_n);
		  }
		if (oneway_tofrom)
		  {
		      /* fetching the OneWay-ToFrom value */
		      tofrom = sqlite3_column_int (stmt, tofrom_n);
		  }
		sprintf (xRowid, FORMAT_64, rowid);
		if (cost <= 0.0)
		  {
		      printf
			  ("ERROR: arc ROWID=%s has NEGATIVE or NULL cost [%1.6f]\n",
			   xRowid, cost);
		      p_graph->error = 1;
		  }
		if (bidirectional)
		  {
		      if (!fromto && !tofrom)
			{
			    if (p_graph->node_code)
				printf
				    ("WARNING: arc forbidden in both directions; ROWID=%s From=%s To=%s\n",
				     xRowid, code_from, code_to);
			    else
			      {
				  sprintf (xIdFrom, FORMAT_64, id_from);
				  sprintf (xIdTo, FORMAT_64, id_to);
				  printf
				      ("WARNING: arc forbidden in both directions; ROWID=%s From=%s To=%s\n",
				       xRowid, xIdFrom, xIdTo);
			      }
			}
		      if (fromto)
			  add_arc (p_graph, rowid, id_from, id_to, code_from,
				   code_to, node_from_x, node_from_y, node_to_x,
				   node_to_y, cost);
		      if (tofrom)
			  add_arc (p_graph, rowid, id_to, id_from, code_to,
				   code_from, node_to_x, node_to_y, node_from_x,
				   node_from_y, cost);
		  }
		else
		    add_arc (p_graph, rowid, id_from, id_to, code_from, code_to,
			     node_from_x, node_from_y, node_to_x, node_to_y,
			     cost);
		if (p_graph->error)
		  {
		      printf ("\n\nERROR: network failed validation\n");
		      printf
			  ("\tyou cannot apply this configuration to build a valid VirtualNetwork\n");
		      sqlite3_finalize (stmt);
		      goto abort;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		sqlite3_finalize (stmt);
		goto abort;
	    }
      }
    sqlite3_finalize (stmt);
    fprintf (stderr, "Step  IV - final evaluation\n");
/* final printout */
    if (p_graph->error)
      {
	  printf ("\n\nERROR: network failed validation\n");
	  printf
	      ("\tyou cannot apply this configuration to build a valid VirtualNetwork\n");
	  fprintf (stderr, "ERROR: VALIDATION FAILURE\n");
      }
    else
      {
	  print_report (p_graph);
	  printf ("\n\nOK: network passed validation\n");
	  printf
	      ("\tyou can apply this configuration to build a valid VirtualNetwork\n");
	  fprintf (stderr, "OK: validation passed\n");
      }
    if (out_table)
      {
	  ret =
	      create_network_data (handle, out_table, force_creation, p_graph,
				   table, from_column, to_column, geom_column,
				   name_column, a_star_supported,
				   min_a_star_coeff);
	  if (ret)
	    {
		printf
		    ("\n\nOK: NETWORK-DATA table '%s' successfully created\n",
		     out_table);
		fprintf (stderr, "OK: table '%s' successfully created\n",
			 out_table);
	    }
	  else
	    {
		printf
		    ("\n\nERROR: creating the NETWORK-DATA table '%s' was not possible\n",
		     out_table);
		fprintf (stderr, "ERROR: table '%s' failure\n", out_table);
	    }
	  if (virt_table)
	    {
		ret =
		    create_virtual_network (handle, out_table, virt_table,
					    force_creation);
		if (ret)
		    fprintf (stderr, "OK: table '%s' successfully created\n",
			     virt_table);
		else
		    fprintf (stderr, "ERROR: table '%s' failure\n", virt_table);
	    }
      }
  abort:
/* disconnecting the SpatiaLite DB */
    ret = sqlite3_close (handle);
    if (ret != SQLITE_OK)
	fprintf (stderr, "sqlite3_close() error: %s\n",
		 sqlite3_errmsg (handle));
    spatialite_cleanup_ex (cache);
    graph_free (p_graph);
}

static void
validate_no_geom (const char *path, const char *table, const char *from_column,
		  const char *to_column, const char *cost_column,
		  const char *name_column, const char *oneway_tofrom,
		  const char *oneway_fromto, int bidirectional,
		  const char *out_table, const char *virt_table,
		  int force_creation)
{
/* performs all the actual network validation - NO-GEOMETRY */
    int ret;
    sqlite3 *handle;
    sqlite3_stmt *stmt;
    struct graph *p_graph = NULL;
    char sql[1024];
    char sql2[128];
    char **results;
    int n_rows;
    int n_columns;
    int i;
    char *err_msg = NULL;
    char *col_name;
    int type;
    int ok_from_column = 0;
    int ok_to_column = 0;
    int ok_cost_column = 0;
    int ok_name_column = 0;
    int ok_oneway_tofrom = 0;
    int ok_oneway_fromto = 0;
    int from_null = 0;
    int from_int = 0;
    int from_double = 0;
    int from_text = 0;
    int from_blob = 0;
    int to_null = 0;
    int to_int = 0;
    int to_double = 0;
    int to_text = 0;
    int to_blob = 0;
    int cost_null = 0;
    int cost_text = 0;
    int cost_blob = 0;
    int tofrom_null = 0;
    int tofrom_double = 0;
    int tofrom_text = 0;
    int tofrom_blob = 0;
    int fromto_null = 0;
    int fromto_double = 0;
    int fromto_text = 0;
    int fromto_blob = 0;
    int col_n;
    int fromto_n;
    int tofrom_n;
    sqlite3_int64 rowid;
    sqlite3_int64 id_from;
    sqlite3_int64 id_to;
    char code_from[1024];
    char code_to[1024];
    double cost;
    int fromto;
    int tofrom;
    char xRowid[128];
    char xIdFrom[128];
    char xIdTo[128];
    void *cache;

/* showing the SQLite version */
    fprintf (stderr, "SQLite version: %s\n", sqlite3_libversion ());
/* showing the SpatiaLite version */
    fprintf (stderr, "SpatiaLite version: %s\n", spatialite_version ());
/* trying to connect the SpatiaLite DB  */
    ret =
	sqlite3_open_v2 (path, &handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", path,
		   sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  return;
      }
    cache = spatialite_alloc_connection ();
    spatialite_init_ex (handle, cache, 0);
    spatialite_autocreate (handle);

    fprintf (stderr, "Step   I - checking for table and columns existence\n");
/* reporting args */
    printf ("\nspatialite-network\n\n");
    printf
	("==================================================================\n");
    printf ("   SpatiaLite db: %s\n", path);
    printf ("validating table: %s\n\n", table);
    printf ("columns layout\n");
    printf
	("==================================================================\n");
    printf ("FromNode: %s\n", from_column);
    printf ("  ToNode: %s\n", to_column);
    printf ("    Cost: %s\n", cost_column);
    if (!name_column)
	printf ("    Name: *unused*\n");
    else
	printf ("    Name: %s\n", name_column);
    printf ("Geometry: *** unsupported ***\n\n");
    if (bidirectional)
      {
	  printf ("assuming arcs to be BIDIRECTIONAL\n");
	  if (oneway_tofrom && oneway_fromto)
	    {
		printf ("OneWay To->From: %s\n", oneway_tofrom);
		printf ("OneWay From->To: %s\n", oneway_fromto);
	    }
      }
    else
	printf ("assuming arcs to be UNIDIRECTIONAL\n");
    if (out_table)
      {
	  printf ("\nNETWORK-DATA table creation required: '%s'\n", out_table);
	  if (virt_table)
	      printf ("\nVirtualNetwork table creation required: '%s'\n",
		      virt_table);
	  if (force_creation)
	      printf ("Overwrite allowed if table already exists\n");
	  else
	      printf ("Overwrite not allowed if table already exists\n");
      }
    else
	printf
	    ("\nsimple validation required\n[NETWORK-DATA table creation is disabled]\n");
    printf
	("==================================================================\n\n");
/* checking for table existence */
    sprintf (sql,
	     "SELECT \"tbl_name\" FROM \"sqlite_master\" WHERE Upper(\"tbl_name\") = Upper('%s') and \"type\" = 'table'",
	     table);
    ret =
	sqlite3_get_table (handle, sql, &results, &n_rows, &n_columns,
			   &err_msg);
    if (ret != SQLITE_OK)
      {
/* some error occurred */
	  fprintf (stderr, "query#1 SQL error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (n_rows == 0)
      {
	  /* required table does not exists */
	  printf ("ERROR: table '%s' does not exists\n", table);
	  goto abort;
      }
    else
	sqlite3_free_table (results);
/* checking for columns existence */
    sprintf (sql, "PRAGMA table_info(\"%s\")", table);
    ret =
	sqlite3_get_table (handle, sql, &results, &n_rows, &n_columns,
			   &err_msg);
    if (ret != SQLITE_OK)
      {
/* some error occurred */
	  fprintf (stderr, "query#2 SQL error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (n_rows > 1)
      {
	  for (i = 1; i <= n_rows; i++)
	    {
		col_name = results[(i * n_columns) + 1];
		if (strcasecmp (from_column, col_name) == 0)
		    ok_from_column = 1;
		if (strcasecmp (to_column, col_name) == 0)
		    ok_to_column = 1;
		if (cost_column)
		  {
		      if (strcasecmp (cost_column, col_name) == 0)
			  ok_cost_column = 1;
		  }
		if (name_column)
		  {
		      if (strcasecmp (name_column, col_name) == 0)
			  ok_name_column = 1;
		  }
		if (oneway_tofrom)
		  {
		      if (strcasecmp (oneway_tofrom, col_name) == 0)
			  ok_oneway_tofrom = 1;
		  }
		if (oneway_fromto)
		  {
		      if (strcasecmp (oneway_fromto, col_name) == 0)
			  ok_oneway_fromto = 1;
		  }
	    }
	  sqlite3_free_table (results);
      }
    if (!ok_from_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		from_column);
    if (!ok_to_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		to_column);
    if (cost_column && !ok_cost_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		cost_column);
    if (name_column && !ok_name_column)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		name_column);
    if (oneway_tofrom && !ok_oneway_tofrom)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		oneway_tofrom);
    if (oneway_fromto && !ok_oneway_fromto)
	printf ("ERROR: column \"%s\".\"%s\" does not exists\n", table,
		oneway_fromto);
    if (!name_column)
	ok_name_column = 1;
    if (ok_from_column && ok_to_column && ok_name_column)
	;
    else
	goto abort;
    if (cost_column && !ok_cost_column)
	goto abort;
    if (oneway_tofrom && !ok_oneway_tofrom)
	goto abort;
    if (oneway_fromto && !ok_oneway_fromto)
	goto abort;
    fprintf (stderr, "Step  II - checking value types consistency\n");
/* checking column types */
    p_graph = graph_init ();
    sprintf (sql, "SELECT \"%s\", \"%s\", \"%s\"", from_column, to_column,
	     cost_column);
    col_n = 3;
    if (oneway_tofrom)
      {
	  sprintf (sql2, ", \"%s\"", oneway_tofrom);
	  strcat (sql, sql2);
	  tofrom_n = col_n;
	  col_n++;
      }
    if (oneway_fromto)
      {
	  sprintf (sql2, ", \"%s\"", oneway_fromto);
	  strcat (sql, sql2);
	  fromto_n = col_n;
	  col_n++;
      }
    sprintf (sql2, " FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  printf ("query#4 SQL error: %s\n", sqlite3_errmsg (handle));
	  goto abort;
      }
    n_columns = sqlite3_column_count (stmt);
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		/* the NodeFrom type */
		type = sqlite3_column_type (stmt, 0);
		if (type == SQLITE_NULL)
		    from_null = 1;
		if (type == SQLITE_INTEGER)
		  {
		      from_int = 1;
		      id_from = sqlite3_column_int64 (stmt, 0);
		      insert_node (p_graph, id_from, "", 0);
		  }
		if (type == SQLITE_FLOAT)
		    from_double = 1;
		if (type == SQLITE_TEXT)
		  {
		      from_text = 1;
		      strcpy (code_from,
			      (char *) sqlite3_column_text (stmt, 0));
		      insert_node (p_graph, -1, code_from, 1);
		  }
		if (type == SQLITE_BLOB)
		    from_blob = 1;
		/* the NodeTo type */
		type = sqlite3_column_type (stmt, 1);
		if (type == SQLITE_NULL)
		    to_null = 1;
		if (type == SQLITE_INTEGER)
		  {
		      to_int = 1;
		      id_to = sqlite3_column_int64 (stmt, 1);
		      insert_node (p_graph, id_to, "", 0);
		  }
		if (type == SQLITE_FLOAT)
		    to_double = 1;
		if (type == SQLITE_TEXT)
		  {
		      to_text = 1;
		      strcpy (code_to, (char *) sqlite3_column_text (stmt, 1));
		      insert_node (p_graph, -1, code_to, 1);
		  }
		if (type == SQLITE_BLOB)
		    to_blob = 1;
		/* the Cost type */
		type = sqlite3_column_type (stmt, 2);
		if (type == SQLITE_NULL)
		    cost_null = 1;
		if (type == SQLITE_TEXT)
		    cost_text = 1;
		if (type == SQLITE_BLOB)
		    cost_blob = 1;
		col_n = 3;
		if (oneway_fromto)
		  {
		      /* the FromTo type */
		      type = sqlite3_column_type (stmt, col_n);
		      col_n++;
		      if (type == SQLITE_NULL)
			  fromto_null = 1;
		      if (type == SQLITE_FLOAT)
			  fromto_double = 1;
		      if (type == SQLITE_TEXT)
			  fromto_text = 1;
		      if (type == SQLITE_BLOB)
			  fromto_blob = 1;
		  }
		if (oneway_tofrom)
		  {
		      /* the ToFrom type */
		      type = sqlite3_column_type (stmt, col_n);
		      col_n++;
		      if (type == SQLITE_NULL)
			  tofrom_null = 1;
		      if (type == SQLITE_FLOAT)
			  tofrom_double = 1;
		      if (type == SQLITE_TEXT)
			  tofrom_text = 1;
		      if (type == SQLITE_BLOB)
			  tofrom_blob = 1;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		sqlite3_finalize (stmt);
		goto abort;
	    }
      }
    sqlite3_finalize (stmt);
    ret = 1;
    if (from_null)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n", table,
		  from_column);
	  ret = 0;
      }
    if (from_blob)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n", table,
		  from_column);
	  ret = 0;
      }
    if (from_double)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n", table,
		  from_column);
	  ret = 0;
      }
    if (to_null)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n", table,
		  to_column);
	  ret = 0;
      }
    if (to_blob)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n", table,
		  to_column);
	  ret = 0;
      }
    if (to_double)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n", table,
		  to_column);
	  ret = 0;
      }
    if (cost_null)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n",
		  table, cost_column);
	  ret = 0;
      }
    if (cost_blob)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n",
		  table, cost_column);
	  ret = 0;
      }
    if (cost_text)
      {
	  printf ("ERROR: column \"%s\".\"%s\" contains TEXT values\n",
		  table, cost_column);
	  ret = 0;
      }
    if (oneway_fromto)
      {
	  if (fromto_null)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n",
			table, oneway_fromto);
		ret = 0;
	    }
	  if (fromto_blob)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n",
			table, oneway_fromto);
		ret = 0;
	    }
	  if (fromto_text)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains TEXT values\n",
			table, oneway_fromto);
		ret = 0;
	    }
	  if (fromto_double)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n",
			table, oneway_fromto);
		ret = 0;
	    }
      }
    if (oneway_tofrom)
      {
	  if (tofrom_null)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains NULL values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
	  if (tofrom_blob)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains BLOB values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
	  if (tofrom_text)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains TEXT values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
	  if (tofrom_double)
	    {
		printf ("ERROR: column \"%s\".\"%s\" contains DOUBLE values\n",
			table, oneway_tofrom);
		ret = 0;
	    }
      }
    if (!ret)
	goto abort;
    if (from_int && to_int)
      {
	  /* each node is identified by an INTEGER id */
	  p_graph->node_code = 0;
      }
    else if (from_text && to_text)
      {
	  /* each node is identified by a TEXT code */
	  p_graph->node_code = 1;
      }
    else
      {
	  printf ("ERROR: NodeFrom / NodeTo have different value types\n");
	  goto abort;
      }
    init_nodes (p_graph);
    fprintf (stderr, "Step III - checking topological consistency\n");
/* checking topological consistency */
    sprintf (sql,
	     "SELECT ROWID, \"%s\", \"%s\", \"%s\"",
	     from_column, to_column, cost_column);
    col_n = 4;
    if (oneway_tofrom)
      {
	  sprintf (sql2, ", \"%s\"", oneway_tofrom);
	  strcat (sql, sql2);
	  tofrom_n = col_n;
	  col_n++;
      }
    if (oneway_fromto)
      {
	  sprintf (sql2, ", \"%s\"", oneway_fromto);
	  strcat (sql, sql2);
	  fromto_n = col_n;
	  col_n++;
      }
    sprintf (sql2, " FROM \"%s\"", table);
    strcat (sql, sql2);
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  printf ("query#4 SQL error: %s\n", sqlite3_errmsg (handle));
	  goto abort;
      }
    n_columns = sqlite3_column_count (stmt);
    while (1)
      {
	  ret = sqlite3_step (stmt);
	  if (ret == SQLITE_DONE)
	      break;
	  if (ret == SQLITE_ROW)
	    {
		fromto = 1;
		tofrom = 1;
		if (p_graph->node_code)
		  {
		      id_from = -1;
		      id_to = -1;
		  }
		else
		  {
		      *code_from = '\0';
		      *code_to = '\0';
		  }
		/* fetching the ROWID */
		rowid = sqlite3_column_int64 (stmt, 0);
		/* fetching the NodeFrom value */
		if (p_graph->node_code)
		    strcpy (code_from, (char *) sqlite3_column_text (stmt, 1));
		else
		    id_from = sqlite3_column_int64 (stmt, 1);
		/* fetching the NodeTo value */
		if (p_graph->node_code)
		    strcpy (code_to, (char *) sqlite3_column_text (stmt, 2));
		else
		    id_to = sqlite3_column_int64 (stmt, 2);
		/* fetching the Cost value */
		cost = sqlite3_column_double (stmt, 3);
		if (oneway_fromto)
		  {
		      /* fetching the OneWay-FromTo value */
		      fromto = sqlite3_column_int (stmt, fromto_n);
		  }
		if (oneway_tofrom)
		  {
		      /* fetching the OneWay-ToFrom value */
		      tofrom = sqlite3_column_int (stmt, tofrom_n);
		  }
		sprintf (xRowid, FORMAT_64, rowid);
		if (cost <= 0.0)
		  {
		      printf
			  ("ERROR: arc ROWID=%s has NEGATIVE or NULL cost [%1.6f]\n",
			   xRowid, cost);
		      p_graph->error = 1;
		  }
		if (bidirectional)
		  {
		      if (!fromto && !tofrom)
			{
			    if (p_graph->node_code)
				printf
				    ("WARNING: arc forbidden in both directions; ROWID=%s From=%s To=%s\n",
				     xRowid, code_from, code_to);
			    else
			      {
				  sprintf (xIdFrom, FORMAT_64, id_from);
				  sprintf (xIdTo, FORMAT_64, id_to);
				  printf
				      ("WARNING: arc forbidden in both directions; ROWID=%s From=%s To=%s\n",
				       xRowid, xIdFrom, xIdTo);
			      }
			}
		      if (fromto)
			  add_arc (p_graph, rowid, id_from, id_to, code_from,
				   code_to, DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX,
				   cost);
		      if (tofrom)
			  add_arc (p_graph, rowid, id_to, id_from, code_to,
				   code_from, DBL_MAX, DBL_MAX, DBL_MAX,
				   DBL_MAX, cost);
		  }
		else
		    add_arc (p_graph, rowid, id_from, id_to, code_from, code_to,
			     DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX, cost);
		if (p_graph->error)
		  {
		      printf ("\n\nERROR: network failed validation\n");
		      printf
			  ("\tyou cannot apply this configuration to build a valid VirtualNetwork\n");
		      sqlite3_finalize (stmt);
		      goto abort;
		  }
	    }
	  else
	    {
		printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
		sqlite3_finalize (stmt);
		goto abort;
	    }
      }
    sqlite3_finalize (stmt);
    fprintf (stderr, "Step  IV - final evaluation\n");
/* final printout */
    if (p_graph->error)
      {
	  printf ("\n\nERROR: network failed validation\n");
	  printf
	      ("\tyou cannot apply this configuration to build a valid VirtualNetwork\n");
	  fprintf (stderr, "ERROR: VALIDATION FAILURE\n");
      }
    else
      {
	  print_report (p_graph);
	  printf ("\n\nOK: network passed validation\n");
	  printf
	      ("\tyou can apply this configuration to build a valid VirtualNetwork\n");
	  fprintf (stderr, "OK: validation passed\n");
      }
    if (out_table)
      {
	  ret =
	      create_network_data (handle, out_table, force_creation, p_graph,
				   table, from_column, to_column, NULL,
				   name_column, 0, DBL_MAX);
	  if (ret)
	    {
		printf
		    ("\n\nOK: NETWORK-DATA table '%s' successfully created\n",
		     out_table);
		fprintf (stderr, "OK: table '%s' successfully created\n",
			 out_table);
		if (virt_table)
		  {
		      ret =
			  create_virtual_network (handle, out_table, virt_table,
						  force_creation);
		      if (ret)
			  fprintf (stderr,
				   "OK: table '%s' successfully created\n",
				   virt_table);
		      else
			  fprintf (stderr, "ERROR: table '%s' failure\n",
				   virt_table);
		  }
	    }
	  else
	    {
		printf
		    ("\n\nERROR: creating the NETWORK-DATA table '%s' was not possible\n",
		     out_table);
		fprintf (stderr, "ERROR: table '%s' failure\n", out_table);
	    }
      }
  abort:
/* disconnecting the SpatiaLite DB */
    ret = sqlite3_close (handle);
    if (ret != SQLITE_OK)
	fprintf (stderr, "sqlite3_close() error: %s\n",
		 sqlite3_errmsg (handle));
    spatialite_cleanup_ex (cache);
    graph_free (p_graph);
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_network ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                      print this help message\n");
    fprintf (stderr,
	     "-d or --db-path pathname          the SpatiaLite db path\n");
    fprintf (stderr,
	     "-T or --table table_name          the db table to be validated\n");
    fprintf (stderr,
	     "-f or --from-column col_name      the column for FromNode\n");
    fprintf (stderr,
	     "-t or --to-column col_name        the column for ToNode\n");
    fprintf (stderr,
	     "-g or --geometry-column col_name  the column for Geometry\n");
    fprintf (stderr, "-c or --cost-column col_name      the column for Cost\n");
    fprintf (stderr,
	     "                                  if omitted, GLength(g)\n");
    fprintf (stderr,
	     "                                  will be used by default\n\n");
    fprintf (stderr, "you can specify the following options as well:\n");
    fprintf (stderr, "----------------------------------------------\n");
    fprintf (stderr, "--a-star-supported                *default*\n");
    fprintf (stderr, "--a-star-excluded\n");
    fprintf (stderr,
	     "-n or --name-column col_name      the column for RoadName\n");
    fprintf (stderr, "--bidirectional                   *default*\n");
    fprintf (stderr, "--unidirectional\n\n");
    fprintf (stderr,
	     "if *bidirectional* each arc connecting FromNode to ToNode is\n");
    fprintf (stderr,
	     "implicitly connecting ToNode to FromNode as well; in this case\n");
    fprintf (stderr, "you can select the following further options:\n");
    fprintf (stderr, "--oneway-tofrom col_name\n");
    fprintf (stderr, "--oneway-fromto col_name\n");
    fprintf (stderr,
	     "both columns are expected to contain BOOLEAN values [1-0];\n");
    fprintf (stderr,
	     "1 means that the arc connection in the given direction is\n");
    fprintf (stderr, "valid, otherwise 0 means a forbidden connection\n\n");
    fprintf (stderr, "in order to create a permanent NETWORK-DATA table\n");
    fprintf (stderr, "you can select the following options:\n");
    fprintf (stderr, "-o or --output-table table_name\n");
    fprintf (stderr, "-v or --virtual-table table_name\n");
    fprintf (stderr, "--overwrite-output\n\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    int i;
    int next_arg = ARG_NONE;
    char *path = NULL;
    char *table = NULL;
    char *from_column = NULL;
    char *to_column = NULL;
    char *cost_column = NULL;
    char *geom_column = NULL;
    char *name_column = NULL;
    char *oneway_tofrom = NULL;
    char *oneway_fromto = NULL;
    char *out_table = NULL;
    char *virt_table = NULL;
    int bidirectional = 1;
    int force_creation = 0;
    int error = 0;
    int a_star_supported = 1;
    for (i = 1; i < argc; i++)
      {
	  /* parsing the invocation arguments */
	  if (next_arg != ARG_NONE)
	    {
		switch (next_arg)
		  {
		  case ARG_DB_PATH:
		      path = argv[i];
		      break;
		  case ARG_TABLE:
		      table = argv[i];
		      break;
		  case ARG_OUT_TABLE:
		      out_table = argv[i];
		      break;
		  case ARG_VIRT_TABLE:
		      virt_table = argv[i];
		      break;
		  case ARG_FROM_COLUMN:
		      from_column = argv[i];
		      break;
		  case ARG_TO_COLUMN:
		      to_column = argv[i];
		      break;
		  case ARG_COST_COLUMN:
		      cost_column = argv[i];
		      break;
		  case ARG_GEOM_COLUMN:
		      geom_column = argv[i];
		      break;
		  case ARG_NAME_COLUMN:
		      name_column = argv[i];
		      break;
		  case ARG_ONEWAY_TOFROM:
		      oneway_tofrom = argv[i];
		      break;
		  case ARG_ONEWAY_FROMTO:
		      oneway_fromto = argv[i];
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
	  if (strcasecmp (argv[i], "--db-path") == 0)
	    {
		next_arg = ARG_DB_PATH;
		continue;
	    }
	  if (strcmp (argv[i], "-d") == 0)
	    {
		next_arg = ARG_DB_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--table") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcmp (argv[i], "-T") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--output-table") == 0)
	    {
		next_arg = ARG_OUT_TABLE;
		continue;
	    }
	  if (strcmp (argv[i], "-o") == 0)
	    {
		next_arg = ARG_OUT_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--virtual-table") == 0)
	    {
		next_arg = ARG_VIRT_TABLE;
		continue;
	    }
	  if (strcmp (argv[i], "-v") == 0)
	    {
		next_arg = ARG_VIRT_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--from-column") == 0)
	    {
		next_arg = ARG_FROM_COLUMN;
		continue;
	    }
	  if (strcmp (argv[i], "-f") == 0)
	    {
		next_arg = ARG_FROM_COLUMN;
		continue;
	    }
	  if (strcasecmp (argv[i], "--to-column") == 0)
	    {
		next_arg = ARG_TO_COLUMN;
		continue;
	    }
	  if (strcmp (argv[i], "-t") == 0)
	    {
		next_arg = ARG_TO_COLUMN;
		continue;
	    }
	  if (strcmp (argv[i], "-c") == 0)
	    {
		next_arg = ARG_COST_COLUMN;
		continue;
	    }
	  if (strcasecmp (argv[i], "--geometry-column") == 0)
	    {
		next_arg = ARG_GEOM_COLUMN;
		continue;
	    }
	  if (strcmp (argv[i], "-g") == 0)
	    {
		next_arg = ARG_GEOM_COLUMN;
		continue;
	    }
	  if (strcasecmp (argv[i], "--name-column") == 0)
	    {
		next_arg = ARG_NAME_COLUMN;
		continue;
	    }
	  if (strcmp (argv[i], "-n") == 0)
	    {
		next_arg = ARG_NAME_COLUMN;
		continue;
	    }
	  if (strcasecmp (argv[i], "--oneway-tofrom") == 0)
	    {
		next_arg = ARG_ONEWAY_TOFROM;
		continue;
	    }
	  if (strcasecmp (argv[i], "--oneway-fromto") == 0)
	    {
		next_arg = ARG_ONEWAY_FROMTO;
		continue;
	    }
	  if (strcasecmp (argv[i], "--bidirectional") == 0)
	    {
		bidirectional = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--overwrite-output") == 0)
	    {
		force_creation = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--unidirectional") == 0)
	    {
		bidirectional = 0;
		continue;
	    }
	  if (strcasecmp (argv[i], "--a-star-excluded") == 0)
	    {
		a_star_supported = 0;
		continue;
	    }
	  if (strcasecmp (argv[i], "--a-star-supported") == 0)
	    {
		a_star_supported = 1;
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
    if (!path)
      {
	  fprintf (stderr, "did you forget setting the --db-path argument ?\n");
	  error = 1;
      }
    if (!table)
      {
	  fprintf (stderr, "did you forget setting the --table argument ?\n");
	  error = 1;
      }
    if (!from_column)
      {
	  fprintf (stderr,
		   "did you forget setting the --from-column argument ?\n");
	  error = 1;
      }
    if (!to_column)
      {
	  fprintf (stderr,
		   "did you forget setting the --to-column argument ?\n");
	  error = 1;
      }
    if (!geom_column)
      {
	  fprintf (stderr,
		   "did you forget setting the --geometry-column argument ?\n");
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }
    if (oneway_tofrom || oneway_fromto)
      {
	  /* using the ONEWAY switches */
	  if (!bidirectional)
	    {
		fprintf (stderr,
			 "using --unidirectional combined with --oneway is forbidden\n");
		error = 1;
	    }
	  if (!oneway_tofrom || !oneway_tofrom)
	    {
		fprintf (stderr,
			 "using --oneway-tofrom requires --oneway-fromto as well\n");
		error = 1;
	    }
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }
    if (strcasecmp (geom_column, "NULL") == 0
	|| strcasecmp (geom_column, "NONE") == 0
	|| strcasecmp (geom_column, "NO") == 0)
      {
	  /* NULL-Geometry has been explicitly requested */
	  geom_column = NULL;
	  a_star_supported = 0;
	  fprintf (stderr, "\nWARNING: a NO-GEOMETRY graph would be processed\n"
		   "the A* algorithm will be consequently disabled.\n\n");
	  if (!cost_column)
	    {
		fprintf (stderr,
			 "NO-GEOMETRY strictly requires to specify some --cost-column argument\n");
		return -1;
	    }
      }
    if (geom_column == NULL)
	validate_no_geom (path, table, from_column, to_column, cost_column,
			  name_column, oneway_tofrom, oneway_fromto,
			  bidirectional, out_table, virt_table, force_creation);
    else
	validate (path, table, from_column, to_column, cost_column, geom_column,
		  name_column, oneway_tofrom, oneway_fromto, bidirectional,
		  out_table, virt_table, force_creation, a_star_supported);
    spatialite_shutdown ();
    return 0;
}
