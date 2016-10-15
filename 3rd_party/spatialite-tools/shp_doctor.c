/* 
/ shp_doctor
/
/ an analysis / sanitizing tool for SHAPEFILES
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

#include <spatialite/gaiageo.h>

#define ARG_NONE		0
#define ARG_IN_PATH		1

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp	_stricmp
#endif /* not WIN32 */

static void
do_analyze (char *base_path, int ignore_shape, int ignore_extent)
{
/* analyzing a SHAPEFILE */
    FILE *fl_shx = NULL;
    FILE *fl_shp = NULL;
    FILE *fl_dbf = NULL;
    char path[1024];
    int rd;
    unsigned char buf_shx[256];
    unsigned char *buf_shp = NULL;
    int buf_size = 1024;
    int shape;
    int x_shape;
    unsigned char bf[1024];
    unsigned char *buf_dbf = NULL;
    int dbf_size;
    int dbf_reclen = 0;
    int dbf_recno;
    int off_dbf;
    int current_row;
    int skpos;
    int offset;
    int off_shp;
    int sz;
    int ind;
    double x;
    double y;
    int points;
    int n;
    int n1;
    int base;
    int start;
    int end;
    int iv;
    double first_x;
    double first_y;
    double last_x;
    double last_y;
    int repeated;
    double shp_minx;
    double shp_miny;
    double shp_maxx;
    double shp_maxy;
    int err_dbf = 0;
    int err_geo = 0;
    char field_name[16];
    char *sys_err;
    int first_coord_err;
    char *err_open = "ERROR: unable to open '%s' for reading: %s\n";
    char *err_header = "Invalid %s header\n";
    char *err_read = "ERROR: invalid read on %s (entity #%d)\n";
    char *err_shape =
	"ERROR: invalid shape-type=%d [expected %d] (entity #%d)\n";
    char *null_shape = "WARNING: NULL shape (entity #%d)\n";
    int endian_arch = gaiaEndianArch ();
    printf ("\nshp_doctor\n\n");
    printf
	("==================================================================\n");
    printf ("input SHP base-path: %s\n", base_path);
    if (ignore_shape)
      {
	  printf ("-option: ignoring shape-type as declared by each entity\n");
	  printf ("\talways using the Shapefile's shape-type by default\n");
      }
    if (ignore_extent)
      {
	  printf ("-option: ignoring BBOX (extent) declarations\n");
	  printf ("\tsuppressing coords consistency check\n");
      }
    printf
	("==================================================================\n\n");
/* opening the SHP file */
    sprintf (path, "%s.shp", base_path);
    fl_shp = fopen (path, "rb");
    if (!fl_shp)
      {
	  sys_err = strerror (errno);
	  printf (err_open, path, sys_err);
      }
/* opening the SHX file */
    sprintf (path, "%s.shx", base_path);
    fl_shx = fopen (path, "rb");
    if (!fl_shx)
      {
	  sys_err = strerror (errno);
	  printf (err_open, path, sys_err);
      }
/* opening the DBF file */
    sprintf (path, "%s.dbf", base_path);
    fl_dbf = fopen (path, "rb");
    if (!fl_dbf)
      {
	  sys_err = strerror (errno);
	  printf (err_open, path, sys_err);
      }
    if (!fl_shp || !fl_shx || !fl_dbf)
	goto no_file;
/* reading SHX file header */
    rd = fread (buf_shx, sizeof (unsigned char), 100, fl_shx);
    if (rd != 100)
      {
	  printf (err_header, "SHX");
	  goto error;
      }
    if (gaiaImport32 (buf_shx + 0, GAIA_BIG_ENDIAN, endian_arch) != 9994)	/* checks the SHX magic number */
      {
	  printf (err_header, "SHX");
	  goto error;
      }
/* reading SHP file header */
    buf_shp = malloc (sizeof (unsigned char) * buf_size);
    rd = fread (buf_shp, sizeof (unsigned char), 100, fl_shp);
    if (rd != 100)
      {
	  printf (err_header, "SHP");
	  goto error;
      }
    if (gaiaImport32 (buf_shp + 0, GAIA_BIG_ENDIAN, endian_arch) != 9994)	/* checks the SHP magic number */
      {
	  printf (err_header, "SHP");
	  goto error;
      }
    shape = gaiaImport32 (buf_shp + 32, GAIA_LITTLE_ENDIAN, endian_arch);
    if (shape == GAIA_SHP_POINT || shape == GAIA_SHP_POINTM
	|| shape == GAIA_SHP_POINTZ || shape == GAIA_SHP_POLYLINE
	|| shape == GAIA_SHP_POLYLINEM || shape == GAIA_SHP_POLYLINEZ
	|| shape == GAIA_SHP_POLYGON || shape == GAIA_SHP_POLYGONM
	|| shape == GAIA_SHP_POLYGONZ || shape == GAIA_SHP_MULTIPOINT
	|| shape == GAIA_SHP_MULTIPOINTM || shape == GAIA_SHP_MULTIPOINTZ)
	;
    else
	goto unsupported;
    if (shape == GAIA_SHP_POINT)
	printf ("shape-type=%d POINT\n\n", shape);
    if (shape == GAIA_SHP_POINTZ)
	printf ("shape-type=%d POINT-Z\n\n", shape);
    if (shape == GAIA_SHP_POINTM)
	printf ("shape-type=%d POINT-M\n\n", shape);
    if (shape == GAIA_SHP_POLYLINE)
	printf ("shape-type=%d POLYLINE\n\n", shape);
    if (shape == GAIA_SHP_POLYLINEZ)
	printf ("shape-type=%d POLYLINE-Z\n\n", shape);
    if (shape == GAIA_SHP_POLYLINEM)
	printf ("shape-type=%d POLYLINE-M\n\n", shape);
    if (shape == GAIA_SHP_POLYGON)
	printf ("shape-type=%d POLYGON\n\n", shape);
    if (shape == GAIA_SHP_POLYGONZ)
	printf ("shape-type=%d POLYGON-Z\n\n", shape);
    if (shape == GAIA_SHP_POLYGONM)
	printf ("shape-type=%d POLYGON-M\n\n", shape);
    if (shape == GAIA_SHP_MULTIPOINT)
	printf ("shape-type=%d MULTIPOINT\n\n", shape);
    if (shape == GAIA_SHP_MULTIPOINTZ)
	printf ("shape-type=%d MULTIPOINT-Z\n\n", shape);
    if (shape == GAIA_SHP_MULTIPOINTM)
	printf ("shape-type=%d MULTIPOINT-M\n\n", shape);
    x_shape = shape;
    shp_minx = gaiaImport64 (buf_shp + 36, GAIA_LITTLE_ENDIAN, endian_arch);
    shp_miny = gaiaImport64 (buf_shp + 44, GAIA_LITTLE_ENDIAN, endian_arch);
    shp_maxx = gaiaImport64 (buf_shp + 52, GAIA_LITTLE_ENDIAN, endian_arch);
    shp_maxy = gaiaImport64 (buf_shp + 60, GAIA_LITTLE_ENDIAN, endian_arch);
    if (!ignore_extent)
      {
	  printf ("shape-extent:\tMIN(x=%1.6f y=%1.6f)\n", shp_minx, shp_miny);
	  printf ("\t\tMAX(x=%1.6f y=%1.6f)\n\n", shp_maxx, shp_maxy);
      }
/* reading DBF file header */
    rd = fread (bf, sizeof (unsigned char), 32, fl_dbf);
    if (rd != 32)
      {
	  printf (err_header, "DBF");
	  goto error;
      }
    if (*bf != 0x03)		/* checks the DBF magic number */
      {
	  printf (err_header, "DBF");
	  goto error;
      }
    dbf_recno = gaiaImport32 (bf + 4, GAIA_LITTLE_ENDIAN, endian_arch);
    dbf_size = gaiaImport16 (bf + 8, GAIA_LITTLE_ENDIAN, endian_arch);
    dbf_reclen = gaiaImport16 (bf + 10, GAIA_LITTLE_ENDIAN, endian_arch);
    printf ("DBF header summary:\n");
    printf ("========================================\n");
    printf ("    # records = %d\n", dbf_recno);
    printf ("record-length = %d\n", dbf_reclen);
    printf ("\nDBF fields:\n");
    printf ("========================================\n");
    dbf_size--;
    off_dbf = 0;
    for (ind = 32; ind < dbf_size; ind += 32)
      {
	  /* fetches DBF fields definitions */
	  rd = fread (bf, sizeof (unsigned char), 32, fl_dbf);
	  if (rd != 32)
	      goto error;
	  memcpy (field_name, bf, 11);
	  field_name[11] = '\0';
	  printf ("name=%-10s offset=%4d type=%c size=%3d decimals=%2d",
		  field_name, off_dbf, *(bf + 11), *(bf + 16), *(bf + 17));
	  switch (*(bf + 11))
	    {
	    case 'C':
		printf (" CHARACTER\n");
		if (*(bf + 16) < 1)
		  {
		      printf ("\t\tERROR: length=0 ???\n");
		      err_dbf = 1;
		  }
		break;
	    case 'N':
		printf (" NUMBER\n");
		if (*(bf + 16) > 18)
		    printf ("\t\tWARNING: expected size is MAX 18 !!!\n");
		break;
	    case 'L':
		printf (" LOGICAL\n");
		if (*(bf + 16) != 1)
		  {
		      printf ("\t\tERROR: expected length is 1 !!!\n");
		      err_dbf = 1;
		  }
		break;
	    case 'D':
		printf (" DATE\n");
		if (*(bf + 16) != 8)
		  {
		      printf ("\t\tERROR: expected length is 8 !!!\n");
		      err_dbf = 1;
		  }
		break;
	    case 'F':
		printf (" FLOAT\n");
		break;
	    default:
		printf (" UNKNOWN\n");
		{
		    printf ("\t\tERROR: unsupported data type\n");
		    err_dbf = 1;
		}
		break;
	    };
	  off_dbf += *(bf + 16);
      }
    buf_dbf = malloc (sizeof (unsigned char) * dbf_reclen);
    buf_shp = malloc (sizeof (unsigned char) * buf_size);
    printf ("\nTesting SHP entities:\n");
    printf ("========================================\n");
    current_row = 0;
    while (1)
      {
	  /* reading entities from shapefile */

	  /* positioning and reading the SHX file */
	  offset = 100 + (current_row * 8);	/* 100 bytes for the header + current row displacement; each SHX row = 8 bytes */
	  skpos = fseek (fl_shx, offset, SEEK_SET);
	  if (skpos != 0)
	      goto eof;
	  rd = fread (bf, sizeof (unsigned char), 8, fl_shx);
	  if (rd != 8)
	      goto eof;
	  off_shp = gaiaImport32 (bf, GAIA_BIG_ENDIAN, endian_arch);
	  /* positioning and reading the DBF file */
	  offset = dbf_size + (current_row * dbf_reclen);
	  skpos = fseek (fl_dbf, offset, SEEK_SET);
	  if (skpos != 0)
	    {
		printf (err_read, "DBF", current_row + 1);
		goto error;
	    }
	  rd = fread (buf_dbf, sizeof (unsigned char), dbf_reclen, fl_dbf);
	  if (rd != dbf_reclen)
	    {
		printf (err_read, "DBF", current_row + 1);
		goto error;
	    }
	  /* positioning and reading corresponding SHP entity - geometry */
	  offset = off_shp * 2;
	  skpos = fseek (fl_shp, offset, SEEK_SET);
	  if (skpos != 0)
	    {
		printf (err_read, "SHP", current_row + 1);
		goto error;
	    }
	  rd = fread (buf_shp, sizeof (unsigned char), 12, fl_shp);
	  if (rd != 12)
	    {
		printf (err_read, "SHP", current_row + 1);
		goto error;
	    }
	  sz = gaiaImport32 (buf_shp + 4, GAIA_BIG_ENDIAN, endian_arch);
	  shape = gaiaImport32 (buf_shp + 8, GAIA_LITTLE_ENDIAN, endian_arch);
	  if (ignore_shape)
	      shape = x_shape;
	  if (shape != x_shape)
	    {
		if (shape == 0)
		    printf (null_shape, current_row + 1);
		else
		  {
		      printf (err_shape, shape, x_shape, current_row + 1);
		      err_geo = 1;
		  }
	    }
	  if ((sz * 2) > buf_size)
	    {
		/* current buffer is too small; we need to allocate a bigger buffer */
		free (buf_shp);
		buf_size = sz * 2;
		buf_shp = malloc (sizeof (unsigned char) * buf_size);
	    }
	  if (shape == GAIA_SHP_POINT || shape == GAIA_SHP_POINTZ
	      || shape == GAIA_SHP_POINTM)
	    {
		/* shape point */
		rd = fread (buf_shp, sizeof (unsigned char), 16, fl_shp);
		if (rd != 16)
		  {
		      printf (err_read, "SHP point-entity", current_row + 1);
		      goto error;
		  }
		x = gaiaImport64 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		y = gaiaImport64 (buf_shp + 8, GAIA_LITTLE_ENDIAN, endian_arch);
		if (!ignore_extent)
		  {
		      if (x < shp_minx || x > shp_maxx || y < shp_miny
			  || y > shp_maxy)
			{
			    printf
				("WARNING: coords outside shp-extent (entity #%d)\n",
				 current_row + 1);
			    printf ("\tx=%1.6f y=%1.6f\n", x, y);
			}
		  }
	    }
	  if (shape == GAIA_SHP_POLYLINE || shape == GAIA_SHP_POLYLINEZ
	      || shape == GAIA_SHP_POLYLINEM)
	    {
		/* shape polyline */
		rd = fread (buf_shp, sizeof (unsigned char), 32, fl_shp);
		if (rd != 32)
		  {
		      printf (err_read, "SHP polyline-entity", current_row + 1);
		      goto error;
		  }
		rd = fread (buf_shp, sizeof (unsigned char), (sz * 2) - 36,
			    fl_shp);
		if (rd != (sz * 2) - 36)
		  {
		      printf (err_read, "SHP polyline-entity", current_row + 1);
		      goto error;
		  }
		n = gaiaImport32 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		n1 = gaiaImport32 (buf_shp + 4, GAIA_LITTLE_ENDIAN,
				   endian_arch);
		base = 8 + (n * 4);
		start = 0;
		first_coord_err = 1;
		for (ind = 0; ind < n; ind++)
		  {
		      if (ind < (n - 1))
			  end =
			      gaiaImport32 (buf_shp + 8 + ((ind + 1) * 4),
					    GAIA_LITTLE_ENDIAN, endian_arch);
		      else
			  end = n1;
		      points = end - start;
		      points = 0;
		      repeated = 0;
		      for (iv = start; iv < end; iv++)
			{
			    x = gaiaImport64 (buf_shp + base + (iv * 16),
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    y = gaiaImport64 (buf_shp + base + (iv * 16) + 8,
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    if (points != 0)
			      {
				  if (last_x == x && last_y == y)
				      repeated = 1;
			      }
			    last_x = x;
			    last_y = y;
			    if (!ignore_extent && first_coord_err)
			      {
				  if (x < shp_minx || x > shp_maxx
				      || y < shp_miny || y > shp_maxy)
				    {
					printf
					    ("WARNING: coords outside shp-extent (entity #%d)\n",
					     current_row + 1);
					printf ("\tx=%1.6f y=%1.6f\n", x, y);
					first_coord_err = 0;
				    }
			      }
			    start++;
			    points++;
			}
		      if (points < 2)
			{
			    printf
				("ERROR: illegal polyline [%d vertices] (entity #%d)\n",
				 points, current_row + 1);
			    err_geo = 1;
			}
		      if (repeated)
			{
			    printf ("WARNING: repeated vertices (entity #%d)\n",
				    current_row + 1);
			    err_geo = 1;
			}
		  }
	    }
	  if (shape == GAIA_SHP_POLYGON || shape == GAIA_SHP_POLYGONZ
	      || shape == GAIA_SHP_POLYGONM)
	    {
		/* shape polygon */
		rd = fread (buf_shp, sizeof (unsigned char), 32, fl_shp);
		if (rd != 32)
		  {
		      printf (err_read, "SHP polygon-entity", current_row + 1);
		      goto error;
		  }
		rd = fread (buf_shp, sizeof (unsigned char), (sz * 2) - 36,
			    fl_shp);
		if (rd != (sz * 2) - 36)
		  {
		      printf (err_read, "SHP polygon-entity", current_row + 1);
		      goto error;
		  }
		n = gaiaImport32 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		n1 = gaiaImport32 (buf_shp + 4, GAIA_LITTLE_ENDIAN,
				   endian_arch);
		base = 8 + (n * 4);
		start = 0;
		first_coord_err = 1;
		repeated = 0;
		for (ind = 0; ind < n; ind++)
		  {
		      if (ind < (n - 1))
			  end =
			      gaiaImport32 (buf_shp + 8 + ((ind + 1) * 4),
					    GAIA_LITTLE_ENDIAN, endian_arch);
		      else
			  end = n1;
		      points = end - start;
		      points = 0;
		      for (iv = start; iv < end; iv++)
			{
			    x = gaiaImport64 (buf_shp + base + (iv * 16),
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    y = gaiaImport64 (buf_shp + base + (iv * 16) + 8,
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    if (points == 0)
			      {
				  first_x = x;
				  first_y = y;
			      }
			    else
			      {
				  if (last_x == x && last_y == y)
				      repeated = 1;
			      }
			    last_x = x;
			    last_y = y;
			    if (!ignore_extent && first_coord_err)
			      {
				  if (x < shp_minx || x > shp_maxx
				      || y < shp_miny || y > shp_maxy)
				    {
					printf
					    ("WARNING: coords outside shp-extent (entity #%d)\n",
					     current_row + 1);
					printf ("\tx=%1.6f y=%1.6f\n", x, y);
					first_coord_err = 0;
				    }
			      }
			    start++;
			    points++;
			}
		      if (points < 3)
			{
			    printf
				("ERROR: illegal ring [%d vertices] (entity #%d)\n",
				 points, current_row + 1);
			    err_geo = 1;
			}
		      else
			{
			    if (first_x == last_x && first_y == last_y)
			      {
				  if (points < 4)
				    {
					printf
					    ("ERROR: illegal ring [%d vertices] (entity #%d)\n",
					     points, current_row + 1);
					err_geo = 1;
				    }
			      }
			    else
			      {
				  printf
				      ("WARNING: unclosed ring (entity #%d)\n",
				       current_row + 1);
				  err_geo = 1;
			      }
			}
		      if (repeated)
			{
			    printf ("WARNING: repeated vertices (entity #%d)\n",
				    current_row + 1);
			    err_geo = 1;
			}
		  }
	    }
	  if (shape == GAIA_SHP_MULTIPOINT || shape == GAIA_SHP_MULTIPOINTZ
	      || shape == GAIA_SHP_MULTIPOINTM)
	    {
		/* shape multipoint */
		rd = fread (buf_shp, sizeof (unsigned char), 32, fl_shp);
		if (rd != 32)
		  {
		      printf (err_read, "SHP multipoint-entity",
			      current_row + 1);
		      goto error;
		  }
		rd = fread (buf_shp, sizeof (unsigned char), (sz * 2) - 36,
			    fl_shp);
		if (rd != (sz * 2) - 36)
		  {
		      printf (err_read, "SHP multipoint-entity",
			      current_row + 1);
		      goto error;
		  }
		n = gaiaImport32 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		first_coord_err = 1;
		for (iv = 0; iv < n; iv++)
		  {
		      x = gaiaImport64 (buf_shp + 4 + (iv * 16),
					GAIA_LITTLE_ENDIAN, endian_arch);
		      y = gaiaImport64 (buf_shp + 4 + (iv * 16) + 8,
					GAIA_LITTLE_ENDIAN, endian_arch);
		      if (!ignore_extent && first_coord_err)
			{
			    if (x < shp_minx || x > shp_maxx || y < shp_miny
				|| y > shp_maxy)
			      {
				  printf
				      ("WARNING: coords outside shp-extent (entity #%d)\n",
				       current_row + 1);
				  printf ("\tx=%1.6f y=%1.6f\n", x, y);
				  first_coord_err = 0;
			      }
			}
		  }
	    }
	  current_row++;
      }
  eof:
    printf ("\nShapefile contains %d entities\n", current_row);
    if (err_dbf)
      {
	  printf ("\n***** DBF contains unsupported data types ********\n");
	  printf ("\tyou can try to sane this issue as follows:\n");
	  printf ("\t- open this DBF using OpenOffice\n");
	  printf ("\t- then save as a new DBF [using a different name]\n");
	  printf ("\t- rename the DBF files in order to set the DBF\n");
	  printf ("\t  exported from OpenOffice as the one associated\n");
	  printf ("\t  with Shapefile\n\n");
	  printf ("\tGOOD LUCK :-)\n");
      }
    if (err_geo)
      {
	  printf ("\n***** SHP contains invalid geometries ********\n");
      }
    if (!err_dbf && !err_geo)
	printf ("\nValidation passed: no problem found\n");
    if (fl_shx)
	fclose (fl_shx);
    if (fl_shp)
	fclose (fl_shp);
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
  no_file:
/* one of shapefile's files can't be accessed */
    printf ("\nUnable to analyze this SHP: some required file is missing\n");
    if (fl_shx)
	fclose (fl_shx);
    if (fl_shp)
	fclose (fl_shp);
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
  error:
/* the shapefile is invalid or corrupted */
    printf ("\nThis Shapefile is corrupted / has an invalid format");
    if (buf_shp)
	free (buf_shp);
    fclose (fl_shx);
    fclose (fl_shp);
    fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
  unsupported:
/* the shapefile has an unrecognized shape type */
    printf ("\nshape-type=%d is not supported", shape);
    if (buf_shp)
	free (buf_shp);
    fclose (fl_shx);
    fclose (fl_shp);
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
}

static void
do_analyze_no_shx (char *base_path, int ignore_shape, int ignore_extent)
{
/* analyzing a SHAPEFILE [ignoring  theSHX] */
    FILE *fl_shp = NULL;
    FILE *fl_dbf = NULL;
    char path[1024];
    int rd;
    unsigned char *buf_shp = NULL;
    int buf_size = 1024;
    int shape;
    int x_shape;
    unsigned char bf[1024];
    unsigned char *buf_dbf = NULL;
    int dbf_size;
    int dbf_reclen = 0;
    int dbf_recno;
    int off_dbf;
    int current_row;
    int skpos;
    int offset;
    int off_shp;
    int sz;
    int ind;
    double x;
    double y;
    int points;
    int n;
    int n1;
    int base;
    int start;
    int end;
    int iv;
    double first_x;
    double first_y;
    double last_x;
    double last_y;
    int repeated;
    double shp_minx;
    double shp_miny;
    double shp_maxx;
    double shp_maxy;
    int err_dbf = 0;
    int err_geo = 0;
    char field_name[16];
    int first_coord_err;
    char *sys_err;
    char *err_open = "ERROR: unable to open '%s' for reading: %s\n";
    char *err_header = "Invalid %s header\n";
    char *err_read = "ERROR: invalid read on %s (entity #%d)\n";
    char *err_shape =
	"ERROR: invalid shape-type=%d [expected %d] (entity #%d)\n";
    char *null_shape = "WARNING: NULL shape (entity #%d)\n";
    int endian_arch = gaiaEndianArch ();
    printf ("\nshp_doctor\n\n");
    printf
	("==================================================================\n");
    printf ("input SHP base-path: %s\n", base_path);
    printf ("-option: ignoring the SHX file\n");
    printf ("\tassuming plain 1:1 correspondence for SHP and DBF entities\n");
    if (ignore_shape)
      {
	  printf ("-option: ignoring shape-type as declared by each entity\n");
	  printf ("\talways using the Shapefile's shape-type by default\n");
      }
    if (ignore_extent)
      {
	  printf ("-option: ignoring BBOX (extent) declarations\n");
	  printf ("\tsuppressing coords consistency check\n");
      }
    printf
	("==================================================================\n\n");
/* opening the SHP file */
    sprintf (path, "%s.shp", base_path);
    fl_shp = fopen (path, "rb");
    if (!fl_shp)
      {
	  sys_err = strerror (errno);
	  printf (err_open, path, sys_err);
      }
/* opening the DBF file */
    sprintf (path, "%s.dbf", base_path);
    fl_dbf = fopen (path, "rb");
    if (!fl_dbf)
      {
	  sys_err = strerror (errno);
	  printf (err_open, path, sys_err);
      }
    if (!fl_shp || !fl_dbf)
	goto no_file;
/* reading SHP file header */
    buf_shp = malloc (sizeof (unsigned char) * buf_size);
    rd = fread (buf_shp, sizeof (unsigned char), 100, fl_shp);
    if (rd != 100)
      {
	  printf (err_header, "SHP");
	  goto error;
      }
    if (gaiaImport32 (buf_shp + 0, GAIA_BIG_ENDIAN, endian_arch) != 9994)	/* checks the SHP magic number */
      {
	  printf (err_header, "SHP");
	  goto error;
      }
    shape = gaiaImport32 (buf_shp + 32, GAIA_LITTLE_ENDIAN, endian_arch);
    if (shape == GAIA_SHP_POINT || shape == GAIA_SHP_POINTM
	|| shape == GAIA_SHP_POINTM || shape == GAIA_SHP_POLYLINE
	|| shape == GAIA_SHP_POLYLINEZ || shape == GAIA_SHP_POLYLINEM
	|| shape == GAIA_SHP_POLYGON || shape == GAIA_SHP_POLYGONZ
	|| shape == GAIA_SHP_POLYGONM || shape == GAIA_SHP_MULTIPOINT
	|| shape == GAIA_SHP_MULTIPOINTZ || shape == GAIA_SHP_MULTIPOINTM)
	;
    else
	goto unsupported;
    if (shape == GAIA_SHP_POINT)
	printf ("shape-type=%d POINT\n\n", shape);
    if (shape == GAIA_SHP_POINTZ)
	printf ("shape-type=%d POINT-Z\n\n", shape);
    if (shape == GAIA_SHP_POINTM)
	printf ("shape-type=%d POINT-M\n\n", shape);
    if (shape == GAIA_SHP_POLYLINE)
	printf ("shape-type=%d POLYLINE\n\n", shape);
    if (shape == GAIA_SHP_POLYLINEZ)
	printf ("shape-type=%d POLYLINE-Z\n\n", shape);
    if (shape == GAIA_SHP_POLYLINEM)
	printf ("shape-type=%d POLYLINE-M\n\n", shape);
    if (shape == GAIA_SHP_POLYGON)
	printf ("shape-type=%d POLYGON\n\n", shape);
    if (shape == GAIA_SHP_POLYGONZ)
	printf ("shape-type=%d POLYGON-Z\n\n", shape);
    if (shape == GAIA_SHP_POLYGONM)
	printf ("shape-type=%d POLYGON-M\n\n", shape);
    if (shape == GAIA_SHP_MULTIPOINT)
	printf ("shape-type=%d MULTIPOINT\n\n", shape);
    if (shape == GAIA_SHP_MULTIPOINTZ)
	printf ("shape-type=%d MULTIPOINT-Z\n\n", shape);
    if (shape == GAIA_SHP_MULTIPOINTM)
	printf ("shape-type=%d MULTIPOINT-M\n\n", shape);
    x_shape = shape;
    shp_minx = gaiaImport64 (buf_shp + 36, GAIA_LITTLE_ENDIAN, endian_arch);
    shp_miny = gaiaImport64 (buf_shp + 44, GAIA_LITTLE_ENDIAN, endian_arch);
    shp_maxx = gaiaImport64 (buf_shp + 52, GAIA_LITTLE_ENDIAN, endian_arch);
    shp_maxy = gaiaImport64 (buf_shp + 60, GAIA_LITTLE_ENDIAN, endian_arch);
    if (!ignore_extent)
      {
	  printf ("shape-extent:\tMIN(x=%1.6f y=%1.6f)\n", shp_minx, shp_miny);
	  printf ("\t\tMAX(x=%1.6f y=%1.6f)\n\n", shp_maxx, shp_maxy);
      }
/* reading DBF file header */
    rd = fread (bf, sizeof (unsigned char), 32, fl_dbf);
    if (rd != 32)
      {
	  printf (err_header, "DBF");
	  goto error;
      }
    if (*bf != 0x03)		/* checks the DBF magic number */
      {
	  printf (err_header, "DBF");
	  goto error;
      }
    dbf_recno = gaiaImport32 (bf + 4, GAIA_LITTLE_ENDIAN, endian_arch);
    dbf_size = gaiaImport16 (bf + 8, GAIA_LITTLE_ENDIAN, endian_arch);
    dbf_reclen = gaiaImport16 (bf + 10, GAIA_LITTLE_ENDIAN, endian_arch);
    printf ("DBF header summary:\n");
    printf ("========================================\n");
    printf ("    # records = %d\n", dbf_recno);
    printf ("record-length = %d\n", dbf_reclen);
    printf ("\nDBF fields:\n");
    printf ("========================================\n");
    dbf_size--;
    off_dbf = 0;
    for (ind = 32; ind < dbf_size; ind += 32)
      {
	  /* fetches DBF fields definitions */
	  rd = fread (bf, sizeof (unsigned char), 32, fl_dbf);
	  if (rd != 32)
	      goto error;
	  memcpy (field_name, bf, 11);
	  field_name[11] = '\0';
	  printf ("name=%-10s offset=%4d type=%c size=%3d decimals=%2d",
		  field_name, off_dbf, *(bf + 11), *(bf + 16), *(bf + 17));
	  switch (*(bf + 11))
	    {
	    case 'C':
		printf (" CHARACTER\n");
		if (*(bf + 16) < 1)
		  {
		      printf ("\t\tERROR: length=0 ???\n");
		      err_dbf = 1;
		  }
		break;
	    case 'N':
		printf (" NUMBER\n");
		if (*(bf + 16) > 18)
		    printf ("\t\tWARNING: expected size is MAX 18 !!!\n");
		break;
	    case 'L':
		printf (" LOGICAL\n");
		if (*(bf + 16) != 1)
		  {
		      printf ("\t\tERROR: expected length is 1 !!!\n");
		      err_dbf = 1;
		  }
		break;
	    case 'D':
		printf (" DATE\n");
		if (*(bf + 16) != 8)
		  {
		      printf ("\t\tERROR: expected length is 8 !!!\n");
		      err_dbf = 1;
		  }
		break;
	    case 'F':
		printf (" FLOAT\n");
		break;
	    default:
		printf (" UNKNOWN\n");
		{
		    printf ("\t\tERROR: unsupported data type\n");
		    err_dbf = 1;
		}
		break;
	    };
	  off_dbf += *(bf + 16);
      }
    buf_dbf = malloc (sizeof (unsigned char) * dbf_reclen);
    buf_shp = malloc (sizeof (unsigned char) * buf_size);
    current_row = 0;
/* positioning the DBF file on first entity */
    offset = dbf_size;
    skpos = fseek (fl_dbf, offset, SEEK_SET);
    if (skpos != 0)
      {
	  printf (err_read, "DBF", current_row + 1);
	  goto error;
      }
    off_shp = 100;
    printf ("\nTesting SHP entities:\n");
    printf ("========================================\n");
    while (1)
      {
	  /* reading entities from shapefile */
	  if (current_row == dbf_recno)
	      goto eof;
	  /* sequentially reading the DBF file */
	  rd = fread (buf_dbf, sizeof (unsigned char), dbf_reclen, fl_dbf);
	  if (rd != dbf_reclen)
	    {
		printf (err_read, "DBF", current_row + 1);
		goto error;
	    }
	  /* positioning and reading corresponding SHP entity - geometry */
	  skpos = fseek (fl_shp, off_shp, SEEK_SET);
	  if (skpos != 0)
	    {
		printf (err_read, "SHP", current_row + 1);
		goto error;
	    }
	  rd = fread (buf_shp, sizeof (unsigned char), 12, fl_shp);
	  if (rd != 12)
	    {
		printf (err_read, "SHP", current_row + 1);
		goto error;
	    }
	  sz = gaiaImport32 (buf_shp + 4, GAIA_BIG_ENDIAN, endian_arch);
	  off_shp += (8 + (sz * 2));
	  shape = gaiaImport32 (buf_shp + 8, GAIA_LITTLE_ENDIAN, endian_arch);
	  if (ignore_shape)
	      shape = x_shape;
	  if (shape != x_shape)
	    {
		if (shape == 0)
		    printf (null_shape, current_row + 1);
		else
		  {
		      printf (err_shape, shape, x_shape, current_row + 1);
		      err_geo = 1;
		  }
	    }
	  if ((sz * 2) > buf_size)
	    {
		/* current buffer is too small; we need to allocate a bigger buffer */
		free (buf_shp);
		buf_size = sz * 2;
		buf_shp = malloc (sizeof (unsigned char) * buf_size);
	    }
	  if (shape == GAIA_SHP_POINT || shape == GAIA_SHP_POINTZ
	      || shape == GAIA_SHP_POINTM)
	    {
		/* shape point */
		rd = fread (buf_shp, sizeof (unsigned char), 16, fl_shp);
		if (rd != 16)
		  {
		      printf (err_read, "SHP point-entity", current_row + 1);
		      goto error;
		  }
		x = gaiaImport64 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		y = gaiaImport64 (buf_shp + 8, GAIA_LITTLE_ENDIAN, endian_arch);
		if (!ignore_extent)
		  {
		      if (x < shp_minx || x > shp_maxx || y < shp_miny
			  || y > shp_maxy)
			{
			    printf
				("WARNING: coords outside shp-extent (entity #%d)\n",
				 current_row + 1);
			    printf ("\tx=%1.6f y=%1.6f\n", x, y);
			}
		  }
	    }
	  if (shape == GAIA_SHP_POLYLINE || shape == GAIA_SHP_POLYLINEZ
	      || shape == GAIA_SHP_POLYLINEM)
	    {
		/* shape polyline */
		rd = fread (buf_shp, sizeof (unsigned char), 32, fl_shp);
		if (rd != 32)
		  {
		      printf (err_read, "SHP polyline-entity", current_row + 1);
		      goto error;
		  }
		rd = fread (buf_shp, sizeof (unsigned char), (sz * 2) - 36,
			    fl_shp);
		if (rd != (sz * 2) - 36)
		  {
		      printf (err_read, "SHP polyline-entity", current_row + 1);
		      goto error;
		  }
		n = gaiaImport32 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		n1 = gaiaImport32 (buf_shp + 4, GAIA_LITTLE_ENDIAN,
				   endian_arch);
		base = 8 + (n * 4);
		start = 0;
		first_coord_err = 1;
		for (ind = 0; ind < n; ind++)
		  {
		      if (ind < (n - 1))
			  end =
			      gaiaImport32 (buf_shp + 8 + ((ind + 1) * 4),
					    GAIA_LITTLE_ENDIAN, endian_arch);
		      else
			  end = n1;
		      points = end - start;
		      points = 0;
		      repeated = 0;
		      for (iv = start; iv < end; iv++)
			{
			    x = gaiaImport64 (buf_shp + base + (iv * 16),
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    y = gaiaImport64 (buf_shp + base + (iv * 16) + 8,
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    if (points != 0)
			      {
				  if (last_x == x && last_y == y)
				      repeated = 1;
			      }
			    last_x = x;
			    last_y = y;
			    if (!ignore_extent && first_coord_err)
			      {
				  if (x < shp_minx || x > shp_maxx
				      || y < shp_miny || y > shp_maxy)
				    {
					printf
					    ("WARNING: coords outside shp-extent (entity #%d)\n",
					     current_row + 1);
					printf ("\tx=%1.6f y=%1.6f\n", x, y);
					first_coord_err = 0;
				    }
			      }
			    start++;
			    points++;
			}
		      if (points < 2)
			{
			    printf
				("ERROR: illegal polyline [%d vertices] (entity #%d)\n",
				 points, current_row + 1);
			    err_geo = 1;
			}
		      if (repeated)
			{
			    printf ("WARNING: repeated vertices (entity #%d)\n",
				    current_row + 1);
			    err_geo = 1;
			}
		  }
	    }
	  if (shape == GAIA_SHP_POLYGON || shape == GAIA_SHP_POLYGONZ
	      || shape == GAIA_SHP_POLYGONM)
	    {
		/* shape polygon */
		rd = fread (buf_shp, sizeof (unsigned char), 32, fl_shp);
		if (rd != 32)
		  {
		      printf (err_read, "SHP polygon-entity", current_row + 1);
		      goto error;
		  }
		rd = fread (buf_shp, sizeof (unsigned char), (sz * 2) - 36,
			    fl_shp);
		if (rd != (sz * 2) - 36)
		  {
		      printf (err_read, "SHP polygon-entity", current_row + 1);
		      goto error;
		  }
		n = gaiaImport32 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		n1 = gaiaImport32 (buf_shp + 4, GAIA_LITTLE_ENDIAN,
				   endian_arch);
		base = 8 + (n * 4);
		start = 0;
		first_coord_err = 1;
		repeated = 0;
		for (ind = 0; ind < n; ind++)
		  {
		      if (ind < (n - 1))
			  end =
			      gaiaImport32 (buf_shp + 8 + ((ind + 1) * 4),
					    GAIA_LITTLE_ENDIAN, endian_arch);
		      else
			  end = n1;
		      points = end - start;
		      points = 0;
		      for (iv = start; iv < end; iv++)
			{
			    x = gaiaImport64 (buf_shp + base + (iv * 16),
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    y = gaiaImport64 (buf_shp + base + (iv * 16) + 8,
					      GAIA_LITTLE_ENDIAN, endian_arch);
			    if (points == 0)
			      {
				  first_x = x;
				  first_y = y;
			      }
			    else
			      {
				  if (last_x == x && last_y == y)
				      repeated = 1;
			      }
			    last_x = x;
			    last_y = y;
			    if (!ignore_extent && first_coord_err)
			      {
				  if (x < shp_minx || x > shp_maxx
				      || y < shp_miny || y > shp_maxy)
				    {
					printf
					    ("WARNING: coords outside shp-extent (entity #%d)\n",
					     current_row + 1);
					printf ("\tx=%1.6f y=%1.6f\n", x, y);
					first_coord_err = 0;
				    }
			      }
			    start++;
			    points++;
			}
		      if (points < 3)
			{
			    printf
				("ERROR: illegal ring [%d vertices] (entity #%d)\n",
				 points, current_row + 1);
			    err_geo = 1;
			}
		      else
			{
			    if (first_x == last_x && first_y == last_y)
			      {
				  if (points < 4)
				    {
					printf
					    ("ERROR: illegal ring [%d vertices] (entity #%d)\n",
					     points, current_row + 1);
					err_geo = 1;
				    }
			      }
			    else
			      {
				  printf
				      ("WARNING: unclosed ring (entity #%d)\n",
				       current_row + 1);
				  err_geo = 1;
			      }
			}
		      if (repeated)
			{
			    printf ("WARNING: repeated vertices (entity #%d)\n",
				    current_row + 1);
			    err_geo = 1;
			}
		  }
	    }
	  if (shape == GAIA_SHP_MULTIPOINT || shape == GAIA_SHP_MULTIPOINTZ
	      || shape == GAIA_SHP_MULTIPOINTM)
	    {
		/* shape multipoint */
		rd = fread (buf_shp, sizeof (unsigned char), 32, fl_shp);
		if (rd != 32)
		  {
		      printf (err_read, "SHP multipoint-entity",
			      current_row + 1);
		      goto error;
		  }
		rd = fread (buf_shp, sizeof (unsigned char), (sz * 2) - 36,
			    fl_shp);
		if (rd != (sz * 2) - 36)
		  {
		      printf (err_read, "SHP multipoint-entity",
			      current_row + 1);
		      goto error;
		  }
		n = gaiaImport32 (buf_shp, GAIA_LITTLE_ENDIAN, endian_arch);
		first_coord_err = 1;
		for (iv = 0; iv < n; iv++)
		  {
		      x = gaiaImport64 (buf_shp + 4 + (iv * 16),
					GAIA_LITTLE_ENDIAN, endian_arch);
		      y = gaiaImport64 (buf_shp + 4 + (iv * 16) + 8,
					GAIA_LITTLE_ENDIAN, endian_arch);
		      if (!ignore_extent && first_coord_err)
			{
			    if (x < shp_minx || x > shp_maxx || y < shp_miny
				|| y > shp_maxy)
			      {
				  printf
				      ("WARNING: coords outside shp-extent (entity #%d)\n",
				       current_row + 1);
				  printf ("\tx=%1.6f y=%1.6f\n", x, y);
				  first_coord_err = 0;
			      }
			}
		  }
	    }
	  current_row++;
      }
  eof:
    printf ("\nShapefile contains %d entities\n", current_row);
    if (err_dbf)
      {
	  printf ("\n***** DBF contains unsupported data types ********\n");
	  printf ("\tyou can try to sane this issue as follows:\n");
	  printf ("\t- open this DBF using OpenOffice\n");
	  printf ("\t- then save as a new DBF [using a different name]\n");
	  printf ("\t- rename the DBF files in order to set the DBF\n");
	  printf ("\t  exported from OpenOffice as the one associated\n");
	  printf ("\t  with Shapefile\n\n");
	  printf ("\tGOOD LUCK :-)\n");
      }
    if (err_geo)
      {
	  printf ("\n***** SHP contains invalid geometries ********\n");
      }
    if (!err_dbf && !err_geo)
	printf ("\nValidation passed: no problem found\n");
    if (fl_shp)
	fclose (fl_shp);
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
  no_file:
/* one of shapefile's files can't be accessed */
    printf ("\nUnable to analyze this SHP: some required file is missing\n");
    if (fl_shp)
	fclose (fl_shp);
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
  error:
/* the shapefile is invalid or corrupted */
    printf ("\nThis Shapefile is corrupted / has an invalid format");
    if (fl_shp)
	fclose (fl_shp);
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
  unsupported:
/* the shapefile has an unrecognized shape type */
    printf ("\nshape-type=%d is not supported", shape);
    if (buf_shp)
	free (buf_shp);
    if (fl_shp)
	fclose (fl_shp);
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    if (buf_shp)
	free (buf_shp);
    return;
}

static void
do_analyze_dbf (char *base_path)
{
/* analyzing a DBF */
    FILE *fl_dbf = NULL;
    char path[1024];
    int rd;
    unsigned char bf[1024];
    unsigned char *buf_dbf = NULL;
    int dbf_size;
    int dbf_reclen = 0;
    int dbf_recno;
    int off_dbf;
    int current_row;
    int skpos;
    int offset;
    int ind;
    int err_dbf = 0;
    int err_geo = 0;
    char field_name[16];
    char *sys_err;
    int deleted_rows = 0;
    char *err_open = "ERROR: unable to open '%s' for reading: %s\n";
    char *err_header = "Invalid %s header\n";
    char *err_read = "ERROR: invalid read on %s (entity #%d)\n";
    int endian_arch = gaiaEndianArch ();
    printf ("\nshp_doctor\n\n");
    printf
	("==================================================================\n");
    printf ("input DBF path: %s\n", base_path);
    printf
	("==================================================================\n\n");
/* opening the DBF file */
    sprintf (path, "%s", base_path);
    fl_dbf = fopen (path, "rb");
    if (!fl_dbf)
      {
	  sys_err = strerror (errno);
	  printf (err_open, path, sys_err);
      }
    if (!fl_dbf)
	goto no_file;
/* reading DBF file header */
    rd = fread (bf, sizeof (unsigned char), 32, fl_dbf);
    if (rd != 32)
      {
	  printf (err_header, "DBF");
	  goto error;
      }
    if (*bf != 0x03)		/* checks the DBF magic number */
      {
	  printf (err_header, "DBF");
	  goto error;
      }
    dbf_recno = gaiaImport32 (bf + 4, GAIA_LITTLE_ENDIAN, endian_arch);
    dbf_size = gaiaImport16 (bf + 8, GAIA_LITTLE_ENDIAN, endian_arch);
    dbf_reclen = gaiaImport16 (bf + 10, GAIA_LITTLE_ENDIAN, endian_arch);
    printf ("DBF header summary:\n");
    printf ("========================================\n");
    printf ("    # records = %d\n", dbf_recno);
    printf ("record-length = %d\n", dbf_reclen);
    printf ("\nDBF fields:\n");
    printf ("========================================\n");
    dbf_size--;
    off_dbf = 0;
    for (ind = 32; ind < dbf_size; ind += 32)
      {
	  /* fetches DBF fields definitions */
	  rd = fread (bf, sizeof (unsigned char), 32, fl_dbf);
	  if (rd != 32)
	      goto error;
	  memcpy (field_name, bf, 11);
	  field_name[11] = '\0';
	  printf ("name=%-10s offset=%4d type=%c size=%3d decimals=%2d",
		  field_name, off_dbf, *(bf + 11), *(bf + 16), *(bf + 17));
	  switch (*(bf + 11))
	    {
	    case 'C':
		printf (" CHARACTER\n");
		if (*(bf + 16) < 1)
		  {
		      printf ("\t\tERROR: length=0 ???\n");
		      err_dbf = 1;
		  }
		break;
	    case 'N':
		printf (" NUMBER\n");
		if (*(bf + 16) > 18)
		    printf ("\t\tWARNING: expected size is MAX 18 !!!\n");
		break;
	    case 'L':
		printf (" LOGICAL\n");
		if (*(bf + 16) != 1)
		  {
		      printf ("\t\tERROR: expected length is 1 !!!\n");
		      err_dbf = 1;
		  }
		break;
	    case 'D':
		printf (" DATE\n");
		if (*(bf + 16) != 8)
		  {
		      printf ("\t\tERROR: expected length is 8 !!!\n");
		      err_dbf = 1;
		  }
		break;
	    case 'F':
		printf (" FLOAT\n");
		break;
	    default:
		printf (" UNKNOWN\n");
		{
		    printf ("\t\tERROR: unsupported data type\n");
		    err_dbf = 1;
		}
		break;
	    };
	  off_dbf += *(bf + 16);
      }
    buf_dbf = malloc (sizeof (unsigned char) * dbf_reclen);
    current_row = 0;
/* positioning the DBF file on first entity */
    offset = dbf_size;
    skpos = fseek (fl_dbf, offset, SEEK_SET);
    if (skpos != 0)
      {
	  printf (err_read, "DBF", current_row + 1);
	  goto error;
      }
    printf ("\nTesting DBF rows:\n");
    printf ("========================================\n");
    while (1)
      {
	  /* reading entities from DBF */
	  if (current_row == dbf_recno)
	      goto eof;
	  /* sequentially reading the DBF file */
	  rd = fread (buf_dbf, sizeof (unsigned char), dbf_reclen, fl_dbf);
	  if (rd != dbf_reclen)
	    {
		printf (err_read, "DBF", current_row + 1);
		goto error;
	    }
	  current_row++;
	  if (*buf_dbf == '*')
	      deleted_rows++;
      }
  eof:
    printf ("\nDBF contains %d entities [%d valid / %d deleted]\n", current_row,
	    current_row - deleted_rows, deleted_rows);
    if (err_dbf)
      {
	  printf ("\n***** DBF contains unsupported data types ********\n");
	  printf ("\tyou can try to sane this issue as follows:\n");
	  printf ("\t- open this DBF using OpenOffice\n");
	  printf ("\t- then save as a new DBF [using a different name]\n");
	  printf ("\tGOOD LUCK :-)\n");
      }
    if (!err_geo)
	printf ("\nValidation passed: no problem found\n");
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    return;
  no_file:
/* the DBF file can't be accessed */
    printf ("\nUnable to analyze this DBF: file not existing\n");
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    return;
  error:
/* the DBF is invalid or corrupted */
    printf ("\nThis DBF is corrupted / has an invalid format");
    if (fl_dbf)
	fclose (fl_dbf);
    if (buf_dbf)
	free (buf_dbf);
    return;
}

static void
do_help ()
{
/* printing the argument list */
    fprintf (stderr, "\n\nusage: shp_doctor ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                      print this help message\n");
    fprintf (stderr,
	     "-i or --in-path pathname          the SHP path [no suffix]\n");
    fprintf (stderr, "                                              or\n");
    fprintf (stderr,
	     "                                  the full DBF path [-dbf]\n");
    fprintf (stderr, "\nyou can specify the following options as well\n");
    fprintf (stderr, "--analyze                 *default*\n");
    fprintf (stderr, "--ignore-shape-type       ignore entities' shape-type\n");
    fprintf (stderr, "--ignore-extent           ignore coord consistency\n");
    fprintf (stderr, "--ignore-shx              ignore the SHX file\n");
    fprintf (stderr, "-dbf or --bare-dbf        bare DBF check\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    int i;
    int next_arg = ARG_NONE;
    char *in_path = NULL;
    int analyze = 1;
    int ignore_shape = 0;
    int ignore_extent = 0;
    int ignore_shx = 0;
    int bare_dbf = 0;
    int error = 0;
    for (i = 1; i < argc; i++)
      {
	  /* parsing the invocation arguments */
	  if (next_arg != ARG_NONE)
	    {
		switch (next_arg)
		  {
		  case ARG_IN_PATH:
		      in_path = argv[i];
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
	  if (strcasecmp (argv[i], "--in-path") == 0)
	    {
		next_arg = ARG_IN_PATH;
		continue;
	    }
	  if (strcmp (argv[i], "-i") == 0)
	    {
		next_arg = ARG_IN_PATH;
		continue;
	    }
	  if (strcasecmp (argv[i], "--analyze") == 0)
	    {
		analyze = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--ignore-shape-type") == 0)
	    {
		ignore_shape = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--ignore-extent") == 0)
	    {
		ignore_extent = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--ignore-shx") == 0)
	    {
		ignore_shx = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "-dbf") == 0)
	    {
		bare_dbf = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--bare-dbf") == 0)
	    {
		bare_dbf = 1;
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
    if (!in_path)
      {
	  fprintf (stderr, "did you forget setting the --in-path argument ?\n");
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }
    if (analyze)
      {
	  if (bare_dbf)
	      do_analyze_dbf (in_path);
	  else if (ignore_shx)
	      do_analyze_no_shx (in_path, ignore_shape, ignore_extent);
	  else
	      do_analyze (in_path, ignore_shape, ignore_extent);
      }
    return 0;
}
