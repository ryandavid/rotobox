/* 
/ spatialite_dxf
/
/ an utility CLI tool for DXF import
/
/ version 1.0, 2013 May 19
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
#include <spatialite/gg_dxf.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp	_stricmp
#endif /* not WIN32 */

#define ARG_NONE		0
#define ARG_DB_PATH		1
#define ARG_DXF_PATH		2
#define ARG_SRID		3
#define ARG_DIMS		4
#define ARG_MODE		5
#define ARG_LAYER		6
#define ARG_PREFIX		7
#define ARG_CACHE_SIZE		8
#define ARG_LINKED_RINGS	9
#define ARG_UNLINKED_RINGS	10

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
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatialite_dxf ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr,
	     "-d or --db-path  pathname       the SpatiaLite DB path\n");
    fprintf (stderr, "-x or --dxf-path pathname       the input DXF path\n\n");
    fprintf (stderr, "you can specify the following options as well:\n");
    fprintf (stderr, "----------------------------------------------\n");
    fprintf (stderr,
	     "-s or --srid       num          an explicit SRID value\n");
    fprintf (stderr,
	     "-p or --prefix  layer_prefix    prefix for DB layer names\n");
    fprintf (stderr,
	     "-l or --layer   layer_name      will import a single DXF layer\n");
    fprintf (stderr,
	     "-all or --all-layers            will import all layers (default)\n\n");
    fprintf (stderr,
	     "-distinct or --distinct-layers  respecting individual DXF layers\n");
    fprintf (stderr,
	     "-mixed or --mixed-layers        merging layers altogether by type\n");
    fprintf (stderr,
	     "                                distinct|mixed are mutually\n");
    fprintf (stderr,
	     "                                exclusive; by default: distinct\n\n");
    fprintf (stderr,
	     "-auto or --auto_2d_3d           2D/3D based on input geometries\n");
    fprintf (stderr,
	     "-2d or --force_2d               unconditionally force 2D\n");
    fprintf (stderr,
	     "-3d or --force_3d               unconditionally force 3D\n");
    fprintf (stderr,
	     "                                auto|2d|3d are mutually exclusive\n");
    fprintf (stderr, "                                by default: auto\n\n");
    fprintf (stderr,
	     "-linked or --linked-rings      support linked polygon rings\n");
    fprintf (stderr,
	     "-unlinked or --unlinked-rings  support unlinked polygon rings\n");
    fprintf (stderr,
	     "                               linked|unlinked are mutually exclusive\n");
    fprintf (stderr, "                                by default: none\n\n");
    fprintf (stderr,
	     "-a or --append                 appends to already exixting tables\n\n");
    fprintf (stderr, "--------------------------\n");
    fprintf (stderr,
	     "-m or --in-memory               using IN-MEMORY database\n");
    fprintf (stderr,
	     "-jo or --journal-off            unsafe [but faster] mode\n");
}

int
main (int argc, char *argv[])
{
/* main DXF importer */
    sqlite3 *handle = NULL;
    int ret;
    int i;
    int next_arg = ARG_NONE;
    int error = 0;
    int in_memory = 0;
    int cache_size = 0;
    int journal_off = 0;
    char *db_path = NULL;
    char *dxf_path = NULL;
    char *selected_layer = NULL;
    char *prefix = NULL;
    int linked_rings = 0;
    int unlinked_rings = 0;
    int special_rings = GAIA_DXF_RING_NONE;
    int mode = GAIA_DXF_IMPORT_BY_LAYER;
    int force_dims = GAIA_DXF_AUTO_2D_3D;
    int srid = -1;
    int append = 0;
    gaiaDxfParserPtr dxf = NULL;
    void *cache = spatialite_alloc_connection ();

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
		  case ARG_DXF_PATH:
		      dxf_path = argv[i];
		      break;
		  case ARG_LAYER:
		      selected_layer = argv[i];
		      break;
		  case ARG_PREFIX:
		      prefix = argv[i];
		      break;
		  case ARG_SRID:
		      srid = atoi (argv[i]);
		      break;
		  case ARG_CACHE_SIZE:
		      cache_size = atoi (argv[i]);
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
	  if (strcasecmp (argv[i], "--dxf-path") == 0)
	    {
		next_arg = ARG_DXF_PATH;
		continue;
	    }
	  if (strcmp (argv[i], "-x") == 0)
	    {
		next_arg = ARG_DXF_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--layer") == 0)
	    {
		next_arg = ARG_LAYER;
		continue;
	    }
	  if (strcmp (argv[i], "-l") == 0)
	    {
		next_arg = ARG_LAYER;
		continue;
	    }
	  if (strcasecmp (argv[i], "--prefix") == 0)
	    {
		next_arg = ARG_PREFIX;
		continue;
	    }
	  if (strcmp (argv[i], "-p") == 0)
	    {
		next_arg = ARG_PREFIX;
		continue;
	    }
	  if (strcasecmp (argv[i], "--srid") == 0)
	    {
		next_arg = ARG_SRID;
		continue;
	    }
	  if (strcmp (argv[i], "-s") == 0)
	    {
		next_arg = ARG_SRID;
		continue;
	    }
	  if (strcasecmp (argv[i], "--all-layers") == 0)
	    {
		selected_layer = NULL;
		continue;
	    }
	  if (strcasecmp (argv[i], "-all") == 0)
	    {
		selected_layer = NULL;
		continue;
	    }
	  if (strcasecmp (argv[i], "--mixed-layers") == 0)
	    {
		mode = GAIA_DXF_IMPORT_MIXED;
		continue;
	    }
	  if (strcasecmp (argv[i], "-mixed") == 0)
	    {
		mode = GAIA_DXF_IMPORT_MIXED;
		continue;
	    }
	  if (strcasecmp (argv[i], "--distinct-layers") == 0)
	    {
		mode = GAIA_DXF_IMPORT_BY_LAYER;
		continue;
	    }
	  if (strcasecmp (argv[i], "-distinct") == 0)
	    {
		mode = GAIA_DXF_IMPORT_BY_LAYER;
		continue;
	    }
	  if (strcasecmp (argv[i], "--auto-2d-3d") == 0)
	    {
		force_dims = GAIA_DXF_AUTO_2D_3D;
		continue;
	    }
	  if (strcasecmp (argv[i], "-auto") == 0)
	    {
		force_dims = GAIA_DXF_AUTO_2D_3D;
		continue;
	    }
	  if (strcasecmp (argv[i], "--force-2d") == 0)
	    {
		force_dims = GAIA_DXF_FORCE_2D;
		continue;
	    }
	  if (strcasecmp (argv[i], "-2d") == 0)
	    {
		force_dims = GAIA_DXF_FORCE_2D;
		continue;
	    }
	  if (strcasecmp (argv[i], "--force-3d") == 0)
	    {
		force_dims = GAIA_DXF_FORCE_3D;
		continue;
	    }
	  if (strcasecmp (argv[i], "-3d") == 0)
	    {
		force_dims = GAIA_DXF_FORCE_3D;
		continue;
	    }
	  if (strcasecmp (argv[i], "--linked-rings") == 0
	      || strcmp (argv[i], "-linked") == 0)
	    {
		linked_rings = 1;
		unlinked_rings = 0;
		continue;
	    }
	  if (strcasecmp (argv[i], "--unlinked-rings") == 0
	      || strcmp (argv[i], "-unlinked") == 0)
	    {
		linked_rings = 0;
		unlinked_rings = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--append") == 0
	      || strcmp (argv[i], "-a") == 0)
	    {
		append = 1;
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
    if (!db_path)
      {
	  fprintf (stderr, "did you forget setting the --db-path argument ?\n");
	  error = 1;
      }
    if (!dxf_path)
      {
	  fprintf (stderr,
		   "did you forget setting the --dxf-path argument ?\n");
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }

/* creating the target DB */
    if (in_memory)
	cache_size = 0;
    ret =
	sqlite3_open_v2 (db_path, &handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open DB: %s\n", sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  goto stop;
      }
    if (cache_size > 0)
      {
	  /* setting the CACHE-SIZE */
	  char *sql = sqlite3_mprintf ("PRAGMA cache_size=%d", cache_size);
	  sqlite3_exec (handle, sql, NULL, NULL, NULL);
	  sqlite3_free (sql);
      }
    if (journal_off)
      {
	  /* disabling the journal: unsafe but faster */
	  sqlite3_exec (handle, "PRAGMA journal_mode = OFF", NULL, NULL, NULL);
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
		sqlite3_close (handle);
		sqlite3_close (mem_handle);
		return -1;
	    }
	  backup = sqlite3_backup_init (mem_handle, "main", handle, "main");
	  if (!backup)
	    {
		fprintf (stderr, "cannot load 'MEMORY-DB'\n");
		sqlite3_close (handle);
		sqlite3_close (mem_handle);
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
	  spatialite_init_ex (handle, cache, 0);
	  printf ("\nusing IN-MEMORY database\n");
      }
    else
	spatialite_init_ex (handle, cache, 0);
    spatialite_autocreate (handle);

/* creating a DXF parser */
    if (linked_rings)
	special_rings = GAIA_DXF_RING_LINKED;
    else if (unlinked_rings)
	special_rings = GAIA_DXF_RING_UNLINKED;
    dxf = gaiaCreateDxfParser (srid, force_dims, prefix, selected_layer,
			       special_rings);
    if (dxf == NULL)
	goto stop;
/* attempting to parse the DXF input file */
    if (gaiaParseDxfFile_r (cache, dxf, dxf_path))
      {
	  /* loading into the DB */
	  if (!gaiaLoadFromDxfParser (handle, dxf, mode, append))
	      fprintf (stderr, "DB error while loading: %s\n", dxf_path);
      }
    else
	fprintf (stderr, "Unable to parse: %s\n", dxf_path);

  stop:
/* destroying the DXF parser */
    gaiaDestroyDxfParser (dxf);

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
		return -1;
	    }
	  backup = sqlite3_backup_init (disk_handle, "main", handle, "main");
	  if (!backup)
	    {
		fprintf (stderr, "Backup failure: 'MEMORY-DB' wasn't saved\n");
		sqlite3_close (handle);
		sqlite3_close (disk_handle);
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

/* memory cleanup */
    if (handle != NULL)
      {
	  /* closing the DB connection */
	  ret = sqlite3_close (handle);
	  if (ret != SQLITE_OK)
	      fprintf (stderr, "sqlite3_close() error: %s\n",
		       sqlite3_errmsg (handle));
      }
    spatialite_cleanup_ex (cache);
    spatialite_shutdown ();
    return 0;
}
