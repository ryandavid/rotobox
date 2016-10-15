/* 
/ spatialite_tool
/
/ an utility CLI tool for Shapefile import / export
/
/ version 1.0, 2008 Decmber 11
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
#include <errno.h>

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

#include <spatialite/gaiaaux.h>
#include <spatialite/gaiageo.h>
#include <spatialite.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp	_stricmp
#endif /* not WIN32 */

#define ARG_NONE		0
#define ARG_CMD			1
#define ARG_SHP			2
#define ARG_TXT			3
#define ARG_DBF			4
#define ARG_DB			5
#define ARG_TABLE		6
#define ARG_COL			7
#define ARG_CS			8
#define ARG_SRID		9
#define ARG_TYPE		10

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
do_import_dbf (char *db_path, char *dbf_path, char *table, char *charset)
{
/* importing some DBF */
    int ret;
    int rows;
    sqlite3 *handle;
    void *cache;

/* showing the SQLite version */
    fprintf (stderr, "SQLite version: %s\n", sqlite3_libversion ());
/* showing the SpatiaLite version */
    fprintf (stderr, "SpatiaLite version: %s\n", spatialite_version ());
/* trying to connect the SpatiaLite DB  */
    ret =
	sqlite3_open_v2 (db_path, &handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", db_path,
		   sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  return;
      }
    cache = spatialite_alloc_connection ();
    spatialite_init_ex (handle, cache, 0);
    spatialite_autocreate (handle);
    if (load_dbf (handle, dbf_path, table, charset, 0, &rows, NULL))
	fprintf (stderr, "Inserted %d rows into '%s' from '%s'\n", rows, table,
		 dbf_path);
    else
	fprintf (stderr, "Some ERROR occurred\n");
/* disconnecting the SpatiaLite DB */
    ret = sqlite3_close (handle);
    if (ret != SQLITE_OK)
	fprintf (stderr, "sqlite3_close() error: %s\n",
		 sqlite3_errmsg (handle));
    spatialite_cleanup_ex (cache);
}

static void
do_import_shp (char *db_path, char *shp_path, char *table, char *charset,
	       int srid, char *column, int coerce2d, int compressed)
{
/* importing some SHP */
    int ret;
    int rows;
    sqlite3 *handle;
    void *cache;

/* showing the SQLite version */
    fprintf (stderr, "SQLite version: %s\n", sqlite3_libversion ());
/* showing the SpatiaLite version */
    fprintf (stderr, "SpatiaLite version: %s\n", spatialite_version ());
/* trying to connect the SpatiaLite DB  */
    ret =
	sqlite3_open_v2 (db_path, &handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", db_path,
		   sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  return;
      }
    cache = spatialite_alloc_connection ();
    spatialite_init_ex (handle, cache, 0);
    spatialite_autocreate (handle);
    if (load_shapefile
	(handle, shp_path, table, charset, srid, column, coerce2d, compressed,
	 0, 0, &rows, NULL))
	fprintf (stderr, "Inserted %d rows into '%s' from '%s.shp'\n", rows,
		 table, shp_path);
    else
	fprintf (stderr, "Some ERROR occurred\n");
/* disconnecting the SpatiaLite DB */
    ret = sqlite3_close (handle);
    if (ret != SQLITE_OK)
	fprintf (stderr, "sqlite3_close() error: %s\n",
		 sqlite3_errmsg (handle));
    spatialite_cleanup_ex (cache);
}

static void
do_export (char *db_path, char *shp_path, char *table, char *column,
	   char *charset, char *type)
{
/* exporting some SHP */
    int ret;
    int rows;
    sqlite3 *handle;
    void *cache;

/* showing the SQLite version */
    fprintf (stderr, "SQLite version: %s\n", sqlite3_libversion ());
/* showing the SpatiaLite version */
    fprintf (stderr, "SpatiaLite version: %s\n", spatialite_version ());
/* trying to connect the SpatiaLite DB  */
    ret = sqlite3_open_v2 (db_path, &handle, SQLITE_OPEN_READONLY, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", db_path,
		   sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  return;
      }
    cache = spatialite_alloc_connection ();
    spatialite_init_ex (handle, cache, 0);
    if (dump_shapefile
	(handle, table, column, shp_path, charset, type, 0, &rows, NULL))
	fprintf (stderr, "Exported %d rows into '%s.shp' from '%s'\n", rows,
		 shp_path, table);
    else
	fprintf (stderr, "Some ERROR occurred\n");
/* disconnecting the SpatiaLite DB */
    ret = sqlite3_close (handle);
    if (ret != SQLITE_OK)
	fprintf (stderr, "sqlite3_close() error: %s\n",
		 sqlite3_errmsg (handle));
    spatialite_cleanup_ex (cache);
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: spatitalite_tool CMD ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr, "CMD has to be one of the followings:\n");
    fprintf (stderr, "------------------------------------\n");
    fprintf (stderr,
	     "-h or --help                      print this help message\n");
    fprintf (stderr,
	     "-i or --import                    import [CSV/TXT, DBF or SHP]\n");
    fprintf (stderr,
	     "-e or --export-shp                exporting some shapefile\n");
    fprintf (stderr, "\nsupported ARGs are:\n");
    fprintf (stderr, "-------------------\n");
    fprintf (stderr, "-dbf or --dbf-path pathname       the full DBF path\n");
    fprintf (stderr,
	     "-shp or --shapefile pathname      the shapefile path [NO SUFFIX]\n");
    fprintf (stderr,
	     "-d or --db-path pathname          the SpatiaLite db path\n");
    fprintf (stderr, "-t or --table table_name          the db geotable\n");
    fprintf (stderr, "-g or --geometry-column col_name  the Geometry column\n");
    fprintf (stderr, "-c or --charset charset_name      a charset name\n");
    fprintf (stderr, "-s or --srid SRID                 the SRID\n");
    fprintf (stderr,
	     "--type         [POINT | LINESTRING | POLYGON | MULTIPOINT]\n");
    fprintf (stderr, "\noptional ARGs for SHP import are:\n");
    fprintf (stderr, "---------------------------------\n");
    fprintf (stderr,
	     "-2 or --coerce-2d                  coerce to 2D geoms [x,y]\n");
    fprintf (stderr,
	     "-k or --compressed                 apply geometry compression\n");
    fprintf (stderr, "\nexamples:\n");
    fprintf (stderr, "---------\n");
    fprintf (stderr,
	     "spatialite_tool -i -dbf abc.dbf -d db.sqlite -t tbl -c CP1252\n");
    fprintf (stderr,
	     "spatialite_tool -i -shp abc -d db.sqlite -t tbl -c CP1252 [-s 4326] [-g geom]\n");
    fprintf (stderr,
	     "spatialite_tool -i -shp abc -d db.sqlite -t tbl -c CP1252 [-s 4326] [-2] [-k]\n");
    fprintf (stderr,
	     "spatialite_tool -e -shp abc -d db.sqlite -t tbl -g geom -c CP1252 [--type POINT]\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    int i;
    int next_arg = ARG_NONE;
    char *shp_path = NULL;
    char *dbf_path = NULL;
    char *db_path = NULL;
    char *table = NULL;
    char *column = NULL;
    char *charset = NULL;
    char *type = NULL;
    int srid = -1;
    int import = 0;
    int export = 0;
    int in_shp = 0;
    int in_dbf = 0;
    int coerce2d = 0;
    int compressed = 0;
    int error = 0;
    for (i = 1; i < argc; i++)
      {
	  /* parsing the invocation arguments */
	  if (next_arg != ARG_NONE)
	    {
		switch (next_arg)
		  {
		  case ARG_SHP:
		      shp_path = argv[i];
		      break;
		  case ARG_DBF:
		      dbf_path = argv[i];
		      break;
		  case ARG_DB:
		      db_path = argv[i];
		      break;
		  case ARG_TABLE:
		      table = argv[i];
		      break;
		  case ARG_COL:
		      column = argv[i];
		      break;
		  case ARG_CS:
		      charset = argv[i];
		      break;
		  case ARG_SRID:
		      srid = atoi (argv[i]);
		      break;
		  case ARG_TYPE:
		      type = argv[i];
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
	  if (strcasecmp (argv[i], "--shapefile") == 0)
	    {
		next_arg = ARG_SHP;
		in_shp = 1;
		continue;
	    }
	  if (strcmp (argv[i], "-shp") == 0)
	    {
		next_arg = ARG_SHP;
		in_shp = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--dbf-path") == 0)
	    {
		next_arg = ARG_DBF;
		in_dbf = 1;
		continue;
	    }
	  if (strcmp (argv[i], "-dbf") == 0)
	    {
		next_arg = ARG_DBF;
		in_dbf = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--db-path") == 0)
	    {
		next_arg = ARG_DB;
		continue;
	    }
	  if (strcmp (argv[i], "-d") == 0)
	    {
		next_arg = ARG_DB;
		continue;
	    }
	  if (strcasecmp (argv[i], "--table") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcmp (argv[i], "-t") == 0)
	    {
		next_arg = ARG_TABLE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--geometry-column") == 0)
	    {
		next_arg = ARG_COL;
		continue;
	    }
	  if (strcmp (argv[i], "-g") == 0)
	    {
		next_arg = ARG_COL;
		continue;
	    }
	  if (strcasecmp (argv[i], "--charset") == 0)
	    {
		next_arg = ARG_CS;
		continue;
	    }
	  if (strcasecmp (argv[i], "-c") == 0)
	    {
		next_arg = ARG_CS;
		continue;
	    }
	  if (strcasecmp (argv[i], "--srid") == 0)
	    {
		next_arg = ARG_SRID;
		continue;
	    }
	  if (strcasecmp (argv[i], "-s") == 0)
	    {
		next_arg = ARG_SRID;
		continue;
	    }
	  if (strcasecmp (argv[i], "--type") == 0)
	    {
		next_arg = ARG_TYPE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--import") == 0 ||
	      strcasecmp (argv[i], "-i") == 0)
	    {
		import = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--export-shp") == 0 ||
	      strcasecmp (argv[i], "-e") == 0)
	    {
		export = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--coerce-2d") == 0 ||
	      strcasecmp (argv[i], "-2") == 0)
	    {
		coerce2d = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--compressed-geometries") == 0 ||
	      strcasecmp (argv[i], "-k") == 0)
	    {
		coerce2d = 1;
		continue;
	    }
	  fprintf (stderr, "unknown argument: %s\n", argv[i]);
	  error = 1;
      }
    if ((import + export) != 1)
      {
	  fprintf (stderr, "undefined CMD\n");
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }
/* checking the arguments */
    if (import)
      {
	  /* import SHP */
	  if (!db_path)
	    {
		fprintf (stderr,
			 "did you forget setting the --db-path argument ?\n");
		error = 1;
	    }
	  if ((in_shp + in_dbf) != 1)
	    {
		fprintf (stderr, "undefined IMPORT source: SHP or DBF ?\n");
		error = 1;
	    }
	  if (in_shp && !shp_path)
	    {
		fprintf (stderr,
			 "did you forget setting the --shapefile argument ?\n");
		error = 1;
	    }
	  if (in_dbf && !dbf_path)
	    {
		fprintf (stderr,
			 "did you forget setting the --dbf-path argument ?\n");
		error = 1;
	    }
	  if (!table)
	    {
		fprintf (stderr,
			 "did you forget setting the --table argument ?\n");
		error = 1;
	    }
	  if (!charset)
	    {
		fprintf (stderr,
			 "did you forget setting the --charset argument ?\n");
		error = 1;
	    }
      }
    if (export)
      {
	  /* export SHP */
	  if (!db_path)
	    {
		fprintf (stderr,
			 "did you forget setting the --db-path argument ?\n");
		error = 1;
	    }
	  if (!shp_path)
	    {
		fprintf (stderr,
			 "did you forget setting the --shapefile argument ?\n");
		error = 1;
	    }
	  if (!table)
	    {
		fprintf (stderr,
			 "did you forget setting the --table argument ?\n");
		error = 1;
	    }
	  if (!column)
	    {
		fprintf (stderr,
			 "did you forget setting the --geometry-column argument ?\n");
		error = 1;
	    }
	  if (!charset)
	    {
		fprintf (stderr,
			 "did you forget setting the --charset argument ?\n");
		error = 1;
	    }
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }
    if (import && in_dbf)
	do_import_dbf (db_path, dbf_path, table, charset);
    if (import && in_shp)
	do_import_shp (db_path, shp_path, table, charset, srid, column,
		       coerce2d, compressed);
    if (export)
	do_export (db_path, shp_path, table, column, charset, type);
    spatialite_shutdown ();
    return 0;
}
