/* 
/ exif_loader
/
/ a tool for uploading JPEG/EXIF photos into a DB 
/ preserving full Exif metadata and building Geometry from GPS tags [if present]
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <errno.h>
#include <sys/types.h>

#if defined(_WIN32) && !defined(__MINGW32__)
#include "config-msvc.h"
#else
#include "config.h"
#endif

#if defined(_WIN32) && !defined(__MINGW32__)
#include <io.h>
#include <direct.h>
#else
#include <dirent.h>
#endif

#ifdef SPATIALITE_AMALGAMATION
#include <spatialite/sqlite3.h>
#else
#include <sqlite3.h>
#endif

#include <spatialite/gaiaexif.h>
#include <spatialite/gaiageo.h>
#include <spatialite.h>

#define ARG_NONE			0
#define ARG_DB_PATH			1
#define ARG_DIR				2
#define ARG_FILE			3

#if defined(_WIN32) && !defined(__MINGW32__)
#define strcasecmp	_stricmp
#endif /* not WIN32 */

static sqlite3_int64
getPixelX (gaiaExifTagListPtr tag_list, int *ok)
{
/* trying to retrieve the ExifImageWidth */
    gaiaExifTagPtr tag;
    *ok = 0;
    if (!tag_list)
	return 0;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->TagId == 0xA002)
	    {
		/* ok, this one is the ExifImageWidth tag */
		if (tag->Type == 3 && tag->Count == 1)
		  {
		      *ok = 1;
		      return *(tag->ShortValues + 0);
		  }
		else if (tag->Type == 4 && tag->Count == 1)
		  {
		      *ok = 1;
		      return *(tag->LongValues + 0);
		  }
	    }
	  tag = tag->Next;
      }
    return 0;
}

static sqlite3_int64
getPixelY (gaiaExifTagListPtr tag_list, int *ok)
{
/* trying to retrieve the ExifImageLength */
    gaiaExifTagPtr tag;
    *ok = 0;
    if (!tag_list)
	return 0;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->TagId == 0xA003)
	    {
		/* ok, this one is the ExifImageLength tag */
		if (tag->Type == 3 && tag->Count == 1)
		  {
		      *ok = 1;
		      return *(tag->ShortValues + 0);
		  }
		else if (tag->Type == 4 && tag->Count == 1)
		  {
		      *ok = 1;
		      return *(tag->LongValues + 0);
		  }
	    }
	  tag = tag->Next;
      }
    return 0;
}

static void
getMake (gaiaExifTagListPtr tag_list, char *str, int len, int *ok)
{
/* trying to retrieve the Make */
    gaiaExifTagPtr tag;
    int l;
    *ok = 0;
    if (!tag_list)
	return;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->TagId == 0x010F)
	    {
		/* ok, this one is the Make tag */
		if (tag->Type == 2)
		  {
		      *ok = 1;
		      l = strlen (tag->StringValue);
		      if (len > l)
			  strcpy (str, tag->StringValue);
		      else
			{
			    memset (str, '\0', len);
			    memcpy (str, tag->StringValue, len - 1);
			}
		      return;
		  }
	    }
	  tag = tag->Next;
      }
    return;
}

static void
getModel (gaiaExifTagListPtr tag_list, char *str, int len, int *ok)
{
/* trying to retrieve the Model */
    gaiaExifTagPtr tag;
    int l;
    *ok = 0;
    if (!tag_list)
	return;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->TagId == 0x0110)
	    {
		/* ok, this one is the Model tag */
		if (tag->Type == 2)
		  {
		      *ok = 1;
		      l = strlen (tag->StringValue);
		      if (len > l)
			  strcpy (str, tag->StringValue);
		      else
			{
			    memset (str, '\0', len);
			    memcpy (str, tag->StringValue, len - 1);
			}
		      return;
		  }
	    }
	  tag = tag->Next;
      }
    return;
}

static void
getDate (gaiaExifTagListPtr tag_list, char *str, int len, int *ok)
{
/* trying to retrieve the Date */
    gaiaExifTagPtr tag;
    int l;
    *ok = 0;
    if (!tag_list)
	return;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->TagId == 0x9003)
	    {
		/* ok, this one is the DateTimeOriginal tag */
		if (tag->Type == 2)
		  {
		      *ok = 1;
		      l = strlen (tag->StringValue);
		      if (len > l)
			  strcpy (str, tag->StringValue);
		      else
			{
			    memset (str, '\0', len);
			    memcpy (str, tag->StringValue, len - 1);
			}
		      if (len > 19)
			{
			    str[4] = '-';
			    str[7] = '-';
			}
		      return;
		  }
	    }
	  tag = tag->Next;
      }
    return;
}

static void
getGpsCoords (gaiaExifTagListPtr tag_list, double *longitude, double *latitude,
	      int *ok)
{
/* trying to retrieve the GPS coordinates */
    gaiaExifTagPtr tag;
    char lat_ref = '\0';
    char long_ref = '\0';
    double lat_degs = -DBL_MAX;
    double lat_mins = -DBL_MAX;
    double lat_secs = -DBL_MAX;
    double long_degs = -DBL_MAX;
    double long_mins = -DBL_MAX;
    double long_secs = -DBL_MAX;
    double dblval;
    double sign;
    int xok;
    *ok = 0;
    if (!tag_list)
	return;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->Gps && tag->TagId == 0x01)
	    {
		/* ok, this one is the GPSLatitudeRef tag */
		if (tag->Type == 2)
		    lat_ref = *(tag->StringValue);
	    }
	  if (tag->Gps && tag->TagId == 0x03)
	    {
		/* ok, this one is the GPSLongitudeRef tag */
		if (tag->Type == 2)
		    long_ref = *(tag->StringValue);
	    }
	  if (tag->Gps && tag->TagId == 0x02)
	    {
		/* ok, this one is the GPSLatitude tag */
		if (tag->Type == 5 && tag->Count == 3)
		  {
		      dblval = gaiaExifTagGetRationalValue (tag, 0, &xok);
		      if (xok)
			  lat_degs = dblval;
		      dblval = gaiaExifTagGetRationalValue (tag, 1, &xok);
		      if (xok)
			  lat_mins = dblval;
		      dblval = gaiaExifTagGetRationalValue (tag, 2, &xok);
		      if (xok)
			  lat_secs = dblval;
		  }
	    }
	  if (tag->Gps && tag->TagId == 0x04)
	    {
		/* ok, this one is the GPSLongitude tag */
		if (tag->Type == 5 && tag->Count == 3)
		  {
		      dblval = gaiaExifTagGetRationalValue (tag, 0, &xok);
		      if (xok)
			  long_degs = dblval;
		      dblval = gaiaExifTagGetRationalValue (tag, 1, &xok);
		      if (xok)
			  long_mins = dblval;
		      dblval = gaiaExifTagGetRationalValue (tag, 2, &xok);
		      if (xok)
			  long_secs = dblval;
		  }
	    }
	  tag = tag->Next;
      }
    if ((lat_ref == 'N' || lat_ref == 'S' || long_ref == 'E' || long_ref == 'W')
	&& lat_degs != -DBL_MAX && lat_mins != -DBL_MAX && lat_secs != -DBL_MAX
	&& long_degs != -DBL_MAX && long_mins != -DBL_MAX
	&& long_secs != -DBL_MAX)
      {
	  *ok = 1;
	  if (lat_ref == 'S')
	      sign = -1.0;
	  else
	      sign = 1.0;
	  lat_degs = math_round (lat_degs * 1000000.0);
	  lat_mins = math_round (lat_mins * 1000000.0);
	  lat_secs = math_round (lat_secs * 1000000.0);
	  dblval =
	      math_round (lat_degs + (lat_mins / 60.0) +
			  (lat_secs / 3600.0)) * (sign / 1000000.0);
	  *latitude = dblval;
	  if (long_ref == 'W')
	      sign = -1.0;
	  else
	      sign = 1.0;
	  long_degs = math_round (long_degs * 1000000.0);
	  long_mins = math_round (long_mins * 1000000.0);
	  long_secs = math_round (long_secs * 1000000.0);
	  dblval =
	      math_round (long_degs + (long_mins / 60.0) +
			  (long_secs / 3600.0)) * (sign / 1000000.0);
	  *longitude = dblval;
      }
    return;
}

static void
getGpsSatellites (gaiaExifTagListPtr tag_list, char *str, int len, int *ok)
{
/* trying to retrieve the GPSSatellites */
    gaiaExifTagPtr tag;
    int l;
    *ok = 0;
    if (!tag_list)
	return;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->Gps && tag->TagId == 0x08)
	    {
		/* ok, this one is the GPSSatellites tag */
		if (tag->Type == 2)
		  {
		      *ok = 1;
		      l = strlen (tag->StringValue);
		      if (len > l)
			  strcpy (str, tag->StringValue);
		      else
			{
			    memset (str, '\0', len);
			    memcpy (str, tag->StringValue, len - 1);
			}
		      return;
		  }
	    }
	  tag = tag->Next;
      }
    return;
}

static double
getGpsDirection (gaiaExifTagListPtr tag_list, int *ok)
{
/* trying to retrieve the GPS direction */
    gaiaExifTagPtr tag;
    char dir_ref = '\0';
    double direction = -DBL_MAX;
    double dblval;
    int xok;
    *ok = 0;
    if (!tag_list)
	return direction;
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->Gps && tag->TagId == 0x10)
	    {
		/* ok, this one is the GPSDirectionRef tag */
		if (tag->Type == 2)
		    dir_ref = *(tag->StringValue);
	    }
	  if (tag->Gps && tag->TagId == 0x11)
	    {
		/* ok, this one is the GPSDirection tag */
		if (tag->Type == 5 && tag->Count == 1)
		  {
		      dblval = gaiaExifTagGetRationalValue (tag, 0, &xok);
		      if (xok)
			  direction = dblval;
		  }
	    }
	  tag = tag->Next;
      }
    if ((dir_ref == 'T' || dir_ref == 'M') && direction != -DBL_MAX)
	*ok = 1;
    return direction;
}

static void
getGpsTimestamp (gaiaExifTagListPtr tag_list, char *str, int len, int *ok)
{
/* trying to retrieve the GPS Timestamp */
    gaiaExifTagPtr tag;
    char date[16];
    char timestamp[32];
    double hours = -DBL_MAX;
    double mins = -DBL_MAX;
    double secs = -DBL_MAX;
    double dblval;
    int xok;
    int hh;
    int mm;
    int ss;
    int millis;
    int l;
    *ok = 0;
    if (!tag_list)
	return;
    strcpy (date, "0000-00-00");
    tag = tag_list->First;
    while (tag)
      {
	  if (tag->Gps && tag->TagId == 0x1D)
	    {
		/* ok, this one is the GPSDateStamp tag */
		if (tag->Type == 2)
		  {
		      strcpy (date, tag->StringValue);
		      date[4] = '-';
		      date[7] = '-';
		  }
	    }
	  if (tag->Gps && tag->TagId == 0x07)
	    {
		/* ok, this one is the GPSTimeStamp tag */
		if (tag->Type == 5 && tag->Count == 3)
		  {
		      dblval = gaiaExifTagGetRationalValue (tag, 0, &xok);
		      if (xok)
			  hours = dblval;
		      dblval = gaiaExifTagGetRationalValue (tag, 1, &xok);
		      if (xok)
			  mins = dblval;
		      dblval = gaiaExifTagGetRationalValue (tag, 2, &xok);
		      if (xok)
			  secs = dblval;
		  }
	    }
	  tag = tag->Next;
      }
    if (hours != -DBL_MAX && mins != -DBL_MAX && secs != -DBL_MAX)
      {
	  *ok = 1;
	  hh = (int) floor (hours);
	  mm = (int) floor (mins);
	  ss = (int) floor (secs);
	  millis = (int) ((secs - ss) * 1000);
	  sprintf (timestamp, "%s %02d:%02d:%02d.%03d", date, hh, mm, ss,
		   millis);
	  l = strlen (timestamp);
	  if (len > l)
	      strcpy (str, timestamp);
	  else
	    {
		memset (str, '\0', len);
		memcpy (str, timestamp, len - 1);
	    }
      }
    return;
}

static int
isExifGps (gaiaExifTagListPtr tag_list)
{
/* checks if this one is a GPS-tagged EXIF */
    int gps_lat = 0;
    int gps_long = 0;
    gaiaExifTagPtr pT = tag_list->First;
    while (pT)
      {
	  if (pT->Gps && pT->TagId == 0x04)
	      gps_long = 1;
	  if (pT->Gps && pT->TagId == 0x02)
	      gps_lat = 1;
	  if (gps_long && gps_lat)
	      return 1;
	  pT = pT->Next;
      }
    return 0;
}

static int
updateExifTables (sqlite3 * handle, const unsigned char *blob, int sz,
		  gaiaExifTagListPtr tag_list, int metadata, const char *path)
{
/* inserting an EXIF photo into the DB */
    int i;
    int iv;
    int ok;
    int ok_human;
    char tag_name[128];
    gaiaExifTagPtr pT;
    int ret;
    char sql[1024];
    char human[1024];
    char make[1024];
    char model[1024];
    char satellites[1024];
    char date[32];
    char timestamp[32];
    char *err_msg = NULL;
    sqlite3_stmt *stmt;
    sqlite3_int64 pk = 0;
    sqlite3_int64 val64;
    double dblval;
    char *type_desc;
    double longitude;
    double latitude;
    gaiaGeomCollPtr geom;
    unsigned char *geoblob;
    int geosize;
/* starts a transaction */
    strcpy (sql, "BEGIN");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("BEGIN error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
/* feeding the ExifPhoto table; preparing the SQL statement*/
    strcpy (sql,
	    "INSERT INTO ExifPhoto (PhotoId, Photo, PixelX, PixelY, CameraMake, CameraModel, ");
    strcat (sql,
	    "ShotDateTime, GpsGeometry, GpsDirection, GpsSatellites, GpsTimestamp, FromPath) ");
    strcat (sql,
	    "VALUES (NULL, ?, ?, ?, ?, ?, JulianDay(?), ?, ?, ?, JulianDay(?), ?)");
    ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
    if (ret != SQLITE_OK)
      {
	  printf ("INSERT INTO ExifPhoto error: %s\n", sqlite3_errmsg (handle));
	  goto abort;
      }
    sqlite3_bind_blob (stmt, 1, blob, sz, SQLITE_STATIC);
    val64 = getPixelX (tag_list, &ok);
    if (ok)
	sqlite3_bind_int64 (stmt, 2, val64);
    else
	sqlite3_bind_null (stmt, 2);
    val64 = getPixelY (tag_list, &ok);
    if (ok)
	sqlite3_bind_int64 (stmt, 3, val64);
    else
	sqlite3_bind_null (stmt, 3);
    getMake (tag_list, make, 1024, &ok);
    if (ok)
	sqlite3_bind_text (stmt, 4, make, strlen (make), SQLITE_STATIC);
    else
	sqlite3_bind_null (stmt, 4);
    getModel (tag_list, model, 1024, &ok);
    if (ok)
	sqlite3_bind_text (stmt, 5, model, strlen (model), SQLITE_STATIC);
    else
	sqlite3_bind_null (stmt, 5);
    getDate (tag_list, date, 32, &ok);
    if (ok)
	sqlite3_bind_text (stmt, 6, date, strlen (date), SQLITE_STATIC);
    else
	sqlite3_bind_text (stmt, 6, "0000-00-00 00:00:00", 19, SQLITE_STATIC);
    getGpsCoords (tag_list, &longitude, &latitude, &ok);
    if (ok)
      {
	  geom = gaiaAllocGeomColl ();
	  geom->Srid = 4326;
	  gaiaAddPointToGeomColl (geom, longitude, latitude);
	  gaiaToSpatiaLiteBlobWkb (geom, &geoblob, &geosize);
	  gaiaFreeGeomColl (geom);
	  sqlite3_bind_blob (stmt, 7, geoblob, geosize, SQLITE_TRANSIENT);
	  free (geoblob);
      }
    else
	sqlite3_bind_null (stmt, 7);
    dblval = getGpsDirection (tag_list, &ok);
    if (ok)
	sqlite3_bind_double (stmt, 8, dblval);
    else
	sqlite3_bind_null (stmt, 8);
    getGpsSatellites (tag_list, satellites, 1024, &ok);
    if (ok)
	sqlite3_bind_text (stmt, 9, satellites, strlen (satellites),
			   SQLITE_STATIC);
    else
	sqlite3_bind_null (stmt, 9);
    getGpsTimestamp (tag_list, timestamp, 32, &ok);
    if (ok)
	sqlite3_bind_text (stmt, 10, timestamp, strlen (timestamp),
			   SQLITE_STATIC);
    else
	sqlite3_bind_text (stmt, 10, "0000-00-00 00:00:00", 19, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 11, path, strlen (path), SQLITE_STATIC);
    ret = sqlite3_step (stmt);
    if (ret == SQLITE_DONE || ret == SQLITE_ROW)
	;
    else
      {
	  printf ("sqlite3_step() error: %s\n", sqlite3_errmsg (handle));
	  sqlite3_finalize (stmt);
	  goto abort;
      }
    sqlite3_finalize (stmt);
    pk = sqlite3_last_insert_rowid (handle);
    if (metadata)
      {
	  /* feeding the ExifTags table; preparing the SQL statement */
	  strcpy (sql,
		  "INSERT OR IGNORE INTO ExifTags (PhotoId, TagId, TagName, GpsTag, ValueType, ");
	  strcat (sql, "TypeName, CountValues) VALUES (?, ?, ?, ?, ?, ?, ?)");
	  ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		printf ("INSERT INTO ExifTags error: %s\n",
			sqlite3_errmsg (handle));
		goto abort;
	    }
	  for (i = 0; i < gaiaGetExifTagsCount (tag_list); i++)
	    {
		pT = gaiaGetExifTagByPos (tag_list, i);
		if (pT)
		  {
		      gaiaExifTagGetName (pT, tag_name, 128);
		      switch (gaiaExifTagGetValueType (pT))
			{
			case 1:
			    type_desc = "BYTE";
			    break;
			case 2:
			    type_desc = "STRING";
			    break;
			case 3:
			    type_desc = "SHORT";
			    break;
			case 4:
			    type_desc = "LONG";
			    break;
			case 5:
			    type_desc = "RATIONAL";
			    break;
			case 6:
			    type_desc = "SBYTE";
			    break;
			case 7:
			    type_desc = "UNDEFINED";
			    break;
			case 8:
			    type_desc = "SSHORT";
			    break;
			case 9:
			    type_desc = "SLONG";
			    break;
			case 10:
			    type_desc = "SRATIONAL";
			    break;
			case 11:
			    type_desc = "FLOAT";
			    break;
			case 12:
			    type_desc = "DOUBLE";
			    break;
			default:
			    type_desc = "UNKNOWN";
			    break;
			};
		      /* INSERTing an Exif Tag */
		      sqlite3_reset (stmt);
		      sqlite3_clear_bindings (stmt);
		      sqlite3_bind_int64 (stmt, 1, pk);
		      sqlite3_bind_int (stmt, 2, gaiaExifTagGetId (pT));
		      sqlite3_bind_text (stmt, 3, tag_name, strlen (tag_name),
					 SQLITE_STATIC);
		      sqlite3_bind_int (stmt, 4, gaiaIsExifGpsTag (pT));
		      sqlite3_bind_int (stmt, 5, gaiaExifTagGetValueType (pT));
		      sqlite3_bind_text (stmt, 6, type_desc, strlen (type_desc),
					 SQLITE_STATIC);
		      sqlite3_bind_int (stmt, 7, gaiaExifTagGetNumValues (pT));
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
		  }
	    }
	  sqlite3_finalize (stmt);
	  /* feeding the ExifValues table; preparing the SQL statement */
	  strcpy (sql,
		  "INSERT OR IGNORE INTO ExifValues (PhotoId, TagId, ValueIndex, ByteValue, ");
	  strcat (sql,
		  "StringValue, NumValue, NumValueBis, DoubleValue, HumanReadable) VALUES ");
	  strcat (sql, "(?, ?, ?, ?, ?, ?, ?, ?, ?)");
	  ret = sqlite3_prepare_v2 (handle, sql, strlen (sql), &stmt, NULL);
	  if (ret != SQLITE_OK)
	    {
		printf ("INSERT INTO ExifValues error: %s\n",
			sqlite3_errmsg (handle));
		goto abort;
	    }
	  for (i = 0; i < gaiaGetExifTagsCount (tag_list); i++)
	    {
		pT = gaiaGetExifTagByPos (tag_list, i);
		if (pT)
		  {
		      gaiaExifTagGetHumanReadable (pT, human, 1024, &ok_human);
		      for (iv = 0; iv < gaiaExifTagGetNumValues (pT); iv++)
			{
			    /* INSERTing an Exif Tag */
			    sqlite3_reset (stmt);
			    sqlite3_clear_bindings (stmt);
			    sqlite3_bind_int64 (stmt, 1, pk);
			    sqlite3_bind_int (stmt, 2, gaiaExifTagGetId (pT));
			    sqlite3_bind_int (stmt, 3, iv);
			    if (gaiaExifTagGetValueType (pT) == 1
				|| gaiaExifTagGetValueType (pT) == 6
				|| gaiaExifTagGetValueType (pT) == 7)
			      {
				  sqlite3_bind_blob (stmt, 4, pT->ByteValue,
						     pT->Count, SQLITE_STATIC);
				  sqlite3_bind_null (stmt, 5);
				  sqlite3_bind_null (stmt, 6);
				  sqlite3_bind_null (stmt, 7);
				  sqlite3_bind_null (stmt, 8);
			      }
			    if (gaiaExifTagGetValueType (pT) == 2)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_text (stmt, 5, pT->StringValue,
						     strlen (pT->StringValue),
						     SQLITE_STATIC);
				  sqlite3_bind_null (stmt, 6);
				  sqlite3_bind_null (stmt, 7);
				  sqlite3_bind_null (stmt, 8);
			      }
			    if (gaiaExifTagGetValueType (pT) == 3)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_null (stmt, 5);
				  val64 =
				      gaiaExifTagGetShortValue (pT, iv, &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 6);
				  else
				      sqlite3_bind_int64 (stmt, 6, val64);
				  sqlite3_bind_null (stmt, 7);
				  sqlite3_bind_null (stmt, 8);
			      }
			    if (gaiaExifTagGetValueType (pT) == 4)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_null (stmt, 5);
				  val64 = gaiaExifTagGetLongValue (pT, iv, &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 6);
				  else
				      sqlite3_bind_int64 (stmt, 6, val64);
				  sqlite3_bind_null (stmt, 7);
				  sqlite3_bind_null (stmt, 8);
			      }
			    if (gaiaExifTagGetValueType (pT) == 5)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_null (stmt, 5);
				  val64 =
				      gaiaExifTagGetRational1Value (pT, iv,
								    &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 6);
				  else
				      sqlite3_bind_int64 (stmt, 6, val64);
				  val64 =
				      gaiaExifTagGetRational2Value (pT, iv,
								    &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 7);
				  else
				      sqlite3_bind_int64 (stmt, 7, val64);
				  dblval =
				      gaiaExifTagGetRationalValue (pT, iv, &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 8);
				  else
				      sqlite3_bind_double (stmt, 8, dblval);
			      }
			    if (gaiaExifTagGetValueType (pT) == 9)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_null (stmt, 5);
				  val64 =
				      gaiaExifTagGetSignedLongValue (pT, iv,
								     &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 6);
				  else
				      sqlite3_bind_int64 (stmt, 6, val64);
				  sqlite3_bind_null (stmt, 7);
				  sqlite3_bind_null (stmt, 8);
			      }
			    if (gaiaExifTagGetValueType (pT) == 10)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_null (stmt, 5);
				  val64 =
				      gaiaExifTagGetSignedRational1Value (pT,
									  iv,
									  &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 6);
				  else
				      sqlite3_bind_int64 (stmt, 6, val64);
				  val64 =
				      gaiaExifTagGetSignedRational2Value (pT,
									  iv,
									  &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 7);
				  else
				      sqlite3_bind_int64 (stmt, 7, val64);
				  dblval =
				      gaiaExifTagGetSignedRationalValue (pT, iv,
									 &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 8);
				  else
				      sqlite3_bind_double (stmt, 8, dblval);
			      }
			    if (gaiaExifTagGetValueType (pT) == 11)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_null (stmt, 5);
				  sqlite3_bind_null (stmt, 6);
				  sqlite3_bind_null (stmt, 7);
				  dblval =
				      gaiaExifTagGetFloatValue (pT, iv, &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 8);
				  else
				      sqlite3_bind_double (stmt, 8, dblval);
			      }
			    if (gaiaExifTagGetValueType (pT) == 12)
			      {
				  sqlite3_bind_null (stmt, 4);
				  sqlite3_bind_null (stmt, 5);
				  sqlite3_bind_null (stmt, 6);
				  sqlite3_bind_null (stmt, 7);
				  dblval =
				      gaiaExifTagGetDoubleValue (pT, iv, &ok);
				  if (!ok)
				      sqlite3_bind_null (stmt, 8);
				  else
				      sqlite3_bind_double (stmt, 8, dblval);
			      }
			    if (!ok_human)
				sqlite3_bind_null (stmt, 9);
			    else
				sqlite3_bind_text (stmt, 9, human,
						   strlen (human),
						   SQLITE_STATIC);
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
			    if (gaiaExifTagGetValueType (pT) == 1
				|| gaiaExifTagGetValueType (pT) == 2
				|| gaiaExifTagGetValueType (pT) == 6
				|| gaiaExifTagGetValueType (pT) == 7)
				break;
			    ok_human = 0;
			}
		  }
	    }
	  sqlite3_finalize (stmt);
      }
/* commits the transaction */
    strcpy (sql, "COMMIT");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("COMMIT error: %s\n", err_msg);
	  sqlite3_free (err_msg);
      }
    return 1;
  abort:
/* rolling back the transaction */
    strcpy (sql, "ROLLBACK");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("ROLLBACK error: %s\n", err_msg);
	  sqlite3_free (err_msg);
      }
    return 0;
}

static int
load_file (sqlite3 * handle, const char *file_path, int gps_only, int metadata)
{
/* importing a single EXIF file */
    FILE *fl;
    char msg[256];
    int sz = 0;
    int rd;
    int ok_exif = 0;
    int loaded = 0;
    int gps_skip = 0;
    unsigned char *blob = NULL;
    gaiaExifTagListPtr tag_list = NULL;
    fl = fopen (file_path, "rb");
    if (!fl)
      {
	  sprintf (msg, "exif_loader: cannot open file '%s'", file_path);
	  perror (msg);
	  return 0;
      }
    if (fseek (fl, 0, SEEK_END) == 0)
	sz = ftell (fl);
    if (sz > 14)
      {
	  blob = malloc (sz);
	  rewind (fl);
	  rd = fread (blob, 1, sz, fl);
	  if (rd == sz)
	    {
		tag_list = gaiaGetExifTags (blob, sz);
		if (tag_list)
		  {
		      ok_exif = 1;
		      if (gps_only && !isExifGps (tag_list))
			{
			    gps_skip = 1;
			    goto stop;
			}
		      if (!updateExifTables
			  (handle, blob, sz, tag_list, metadata, file_path))
			  goto stop;
		      loaded = 1;
		  }
	    }
      }
  stop:
    if (!ok_exif)
	printf ("file '%s' doesn't seem to be a valid EXIF file\n", file_path);
    else if (!loaded)
      {
	  if (gps_skip)
	      printf
		  ("file '%s' is a valid EXIF file, but doesn't contain any GPS info\n",
		   file_path);
	  else
	      printf ("SQL error(s): file '%s' was not loaded\n", file_path);
      }
    else
	printf ("file '%s' successfully loaded\n", file_path);
    if (blob)
	free (blob);
    if (tag_list)
	gaiaExifTagsFree (tag_list);
    fclose (fl);
    return loaded;
}

static int
load_dir (sqlite3 * handle, const char *dir_path, int gps_only, int metadata)
{
/* importing EXIF files from a whole DIRECTORY */
#if defined(_WIN32) && !defined(__MINGW32__)
/* Visual Studio .NET */
    struct _finddata_t c_file;
    intptr_t hFile;
    int cnt = 0;
    char file_path[1024];
    if (_chdir (dir_path) < 0)
      {
	  fprintf (stderr, "exif_loader: cannot access dir '%s'", dir_path);
	  return 0;
      }
    if ((hFile = _findfirst ("*.*", &c_file)) == -1L)
	fprintf (stderr, "exif_loader: cannot access dir '%s' [or empty dir]\n",
		 dir_path);
    else
      {
	  while (1)
	    {
		if ((c_file.attrib & _A_RDONLY) == _A_RDONLY
		    || (c_file.attrib & _A_NORMAL) == _A_NORMAL)
		  {
		      sprintf (file_path, "%s\\%s", dir_path, c_file.name);
		      cnt += load_file (handle, file_path, gps_only, metadata);
		  }
		if (_findnext (hFile, &c_file) != 0)
		    break;
	    };
	  _findclose (hFile);
      }
    return cnt;
#else
/* not Visual Studio .NET */
    int cnt = 0;
    char file_path[4096];
    char msg[256];
    struct dirent *entry;
    DIR *dir = opendir (dir_path);
    if (!dir)
      {
	  sprintf (msg, "exif_loader: cannot access dir '%s'", dir_path);
	  perror (msg);
	  return 0;
      }
    while (1)
      {
	  /* scanning dir-entries */
	  entry = readdir (dir);
	  if (!entry)
	      break;
	  sprintf (file_path, "%s/%s", dir_path, entry->d_name);
	  cnt += load_file (handle, file_path, gps_only, metadata);
      }
    closedir (dir);
    return cnt;
#endif
}

static int
checkExifTables (sqlite3 * handle)
{
/* creates the EXIF DB tables / or checks existing ones for validity */
    int ret;
    char sql[1024];
    char *err_msg = NULL;
    int ok_photoId;
    int ok_photo;
    int ok_pixelX;
    int ok_pixelY;
    int ok_cameraMake;
    int ok_cameraModel;
    int ok_shotDateTime;
    int ok_gpsGeometry;
    int ok_gpsDirection;
    int ok_gpsTimestamp;
    int ok_fromPath;
    int ok_tagId;
    int ok_tagName;
    int ok_gpsTag;
    int ok_valueType;
    int ok_typeName;
    int ok_countValues;
    int ok_valueIndex;
    int ok_byteValue;
    int ok_stringValue;
    int ok_numValue;
    int ok_numValueBis;
    int ok_doubleValue;
    int ok_humanReadable;
    int err_pk;
    int ok_photoIdPk;
    int ok_tagIdPk;
    int ok_valueIndexPk;
    int pKey;
    const char *name;
    int i;
    char **results;
    int rows;
    int columns;
/* creating the ExifPhoto table */
    strcpy (sql, "CREATE TABLE IF NOT EXISTS ExifPhoto (\n");
    strcat (sql, "PhotoId INTEGER PRIMARY KEY AUTOINCREMENT,\n");
    strcat (sql, "Photo BLOB NOT NULL,\n");
    strcat (sql, "PixelX INTEGER,\n");
    strcat (sql, "PixelY INTEGER,\n");
    strcat (sql, "CameraMake TEXT,\n");
    strcat (sql, "CameraModel TEXT,\n");
    strcat (sql, "ShotDateTime DOUBLE,\n");
    strcat (sql, "GpsGeometry BLOB, ");
    strcat (sql, "GpsDirection DOUBLE, ");
    strcat (sql, "GpsSatellites TEXT,\n");
    strcat (sql, "GpsTimestamp DOUBLE, ");
    strcat (sql, "FromPath TEXT");
    strcat (sql, ")");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("CREATE TABLE ExifPhoto error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
/* checking the ExifPhoto table for sanity */
    ok_photoId = 0;
    ok_photo = 0;
    ok_pixelX = 0;
    ok_pixelY = 0;
    ok_cameraMake = 0;
    ok_cameraModel = 0;
    ok_shotDateTime = 0;
    ok_gpsGeometry = 0;
    ok_gpsDirection = 0;
    ok_gpsTimestamp = 0;
    ok_fromPath = 0;
    ok_photoIdPk = 0;
    err_pk = 0;
    strcpy (sql, "PRAGMA table_info(\"ExifPhoto\")");
    ret = sqlite3_get_table (handle, sql, &results, &rows, &columns, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("PRAGMA table_info(\"ExifPhoto\") error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (atoi (results[(i * columns) + 5]) == 0)
		    pKey = 0;
		else
		    pKey = 1;
		if (strcasecmp (name, "PhotoId") == 0)
		    ok_photoId = 1;
		if (strcasecmp (name, "Photo") == 0)
		    ok_photo = 1;
		if (strcasecmp (name, "PixelX") == 0)
		    ok_pixelX = 1;
		if (strcasecmp (name, "PixelY") == 0)
		    ok_pixelY = 1;
		if (strcasecmp (name, "CameraMake") == 0)
		    ok_cameraMake = 1;
		if (strcasecmp (name, "CameraModel") == 0)
		    ok_cameraModel = 1;
		if (strcasecmp (name, "ShotDateTime") == 0)
		    ok_shotDateTime = 1;
		if (strcasecmp (name, "GpsGeometry") == 0)
		    ok_gpsGeometry = 1;
		if (strcasecmp (name, "GpsDirection") == 0)
		    ok_gpsDirection = 1;
		if (strcasecmp (name, "GpsTimestamp") == 0)
		    ok_gpsTimestamp = 1;
		if (strcasecmp (name, "FromPath") == 0)
		    ok_fromPath = 1;
		if (pKey)
		  {
		      if (strcasecmp (name, "PhotoId") == 0)
			  ok_photoIdPk = 1;
		      else
			  err_pk = 1;
		  }
	    }
      }
    sqlite3_free_table (results);
    if (ok_photoId && ok_photo && ok_pixelX && ok_pixelY && ok_cameraMake
	&& ok_cameraModel && ok_shotDateTime && ok_gpsGeometry
	&& ok_gpsDirection && ok_gpsTimestamp && ok_fromPath && ok_photoIdPk
	&& !err_pk)
	;
    else
      {
	  printf
	      ("ERROR: table 'ExifPhoto' already exists, but has incompatible columns\n");
	  goto abort;
      }
/* creating the ExifTags table */
    strcpy (sql, "CREATE TABLE IF NOT EXISTS ExifTags (\n");
    strcat (sql, "PhotoId INTEGER NOT NULL,\n");
    strcat (sql, "TagId INTEGER NOT NULL,\n");
    strcat (sql, "TagName TEXT NOT NULL,\n");
    strcat (sql, "GpsTag INTEGER NOT NULL CHECK (GpsTag IN (0, 1)),\n");
    strcat (sql,
	    "ValueType INTEGER NOT NULL CHECK (ValueType IN (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)),\n");
    strcat (sql, "TypeName TEXT NOT NULL,\n");
    strcat (sql, "CountValues INTEGER NOT NULL,\n");
    strcat (sql, "PRIMARY KEY (PhotoId, TagId)");
    strcat (sql, ")");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("CREATE TABLE ExifTags error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
/* checking the ExifTags table for sanity */
    ok_photoId = 0;
    ok_tagId = 0;
    ok_tagName = 0;
    ok_gpsTag = 0;
    ok_valueType = 0;
    ok_typeName = 0;
    ok_countValues = 0;
    ok_photoIdPk = 0;
    ok_tagIdPk = 0;
    err_pk = 0;
    strcpy (sql, "PRAGMA table_info(\"ExifTags\")");
    ret = sqlite3_get_table (handle, sql, &results, &rows, &columns, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("PRAGMA table_info(\"ExifTags\") error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (atoi (results[(i * columns) + 5]) == 0)
		    pKey = 0;
		else
		    pKey = 1;
		if (strcasecmp (name, "PhotoId") == 0)
		    ok_photoId = 1;
		if (strcasecmp (name, "TagId") == 0)
		    ok_tagId = 1;
		if (strcasecmp (name, "TagName") == 0)
		    ok_tagName = 1;
		if (strcasecmp (name, "GpsTag") == 0)
		    ok_gpsTag = 1;
		if (strcasecmp (name, "ValueType") == 0)
		    ok_valueType = 1;
		if (strcasecmp (name, "TypeName") == 0)
		    ok_typeName = 1;
		if (strcasecmp (name, "CountValues") == 0)
		    ok_countValues = 1;
		if (pKey)
		  {
		      if (strcasecmp (name, "PhotoId") == 0)
			  ok_photoIdPk = 1;
		      else if (strcasecmp (name, "TagId") == 0)
			  ok_tagIdPk = 1;
		      else
			  err_pk = 1;
		  }
	    }
      }
    sqlite3_free_table (results);
    if (ok_photoId && ok_tagId && ok_tagName && ok_gpsTag && ok_valueType
	&& ok_typeName && ok_countValues && ok_photoIdPk && ok_tagIdPk
	&& !err_pk)
	;
    else
      {
	  printf
	      ("ERROR: table 'ExifTags' already exists, but has incompatible columns\n");
	  goto abort;
      }
/* creating the ExifValues table */
    strcpy (sql, "CREATE TABLE IF NOT EXISTS ExifValues (\n");
    strcat (sql, "PhotoId INTEGER NOT NULL,\n");
    strcat (sql, "TagId INTEGER NOT NULL,\n");
    strcat (sql, "ValueIndex INTEGER NOT NULL,\n");
    strcat (sql, "ByteValue BLOB,\n");
    strcat (sql, "StringValue TEXT,\n");
    strcat (sql, "NumValue INTEGER,\n");
    strcat (sql, "NumValueBis INTEGER,\n");
    strcat (sql, "DoubleValue DOUBLE,\n");
    strcat (sql, "HumanReadable TEXT,\n");
    strcat (sql, "PRIMARY KEY (PhotoId, TagId, ValueIndex)");
    strcat (sql, ")");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("CREATE TABLE ExifValues error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
/* checking the ExifValues table for sanity */
    ok_photoId = 0;
    ok_tagId = 0;
    ok_valueIndex = 0;
    ok_byteValue = 0;
    ok_stringValue = 0;
    ok_numValue = 0;
    ok_numValueBis = 0;
    ok_doubleValue = 0;
    ok_humanReadable = 0;
    ok_photoIdPk = 0;
    ok_tagIdPk = 0;
    ok_valueIndexPk = 0;
    err_pk = 0;
    strcpy (sql, "PRAGMA table_info(\"ExifValues\")");
    ret = sqlite3_get_table (handle, sql, &results, &rows, &columns, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("PRAGMA table_info(\"ExifValues\") error: %s\n", err_msg);
	  sqlite3_free (err_msg);
	  goto abort;
      }
    if (rows < 1)
	;
    else
      {
	  for (i = 1; i <= rows; i++)
	    {
		name = results[(i * columns) + 1];
		if (atoi (results[(i * columns) + 5]) == 0)
		    pKey = 0;
		else
		    pKey = 1;
		if (strcasecmp (name, "PhotoId") == 0)
		    ok_photoId = 1;
		if (strcasecmp (name, "TagId") == 0)
		    ok_tagId = 1;
		if (strcasecmp (name, "ValueIndex") == 0)
		    ok_valueIndex = 1;
		if (strcasecmp (name, "ByteValue") == 0)
		    ok_byteValue = 1;
		if (strcasecmp (name, "StringValue") == 0)
		    ok_stringValue = 1;
		if (strcasecmp (name, "NumValue") == 0)
		    ok_numValue = 1;
		if (strcasecmp (name, "NumValueBis") == 0)
		    ok_numValueBis = 1;
		if (strcasecmp (name, "DoubleValue") == 0)
		    ok_doubleValue = 1;
		if (strcasecmp (name, "HumanReadable") == 0)
		    ok_humanReadable = 1;
		if (pKey)
		  {
		      if (strcasecmp (name, "PhotoId") == 0)
			  ok_photoIdPk = 1;
		      else if (strcasecmp (name, "TagId") == 0)
			  ok_tagIdPk = 1;
		      else if (strcasecmp (name, "ValueIndex") == 0)
			  ok_valueIndexPk = 1;
		      else
			  err_pk = 1;
		  }
	    }
      }
    sqlite3_free_table (results);
    if (ok_photoId && ok_tagId && ok_valueIndex && ok_byteValue
	&& ok_stringValue && ok_numValue && ok_numValueBis && ok_doubleValue
	&& ok_humanReadable && ok_photoIdPk && ok_tagIdPk && ok_valueIndexPk
	&& !err_pk)
	;
    else
      {
	  printf
	      ("ERROR: table 'ExifValues' already exists, but has incompatible columns\n");
	  goto abort;
      }
/* creating the ExifView view */
    strcpy (sql, "CREATE VIEW IF NOT EXISTS \"ExifMetadata\" AS\n");
    strcat (sql, "SELECT p.\"PhotoId\" AS 'PhotoId', ");
    strcat (sql, "t.\"TagId\" AS 'TagId', ");
    strcat (sql, "t.\"TagName\" AS 'TagName',");
    strcat (sql, "t.\"GpsTag\" AS 'GpsTag',\n");
    strcat (sql, "t.\"ValueType\" AS 'ValueType',");
    strcat (sql, "t.\"TypeName\" AS 'TypeName', ");
    strcat (sql, "t.\"CountValues\" AS 'CountValues', ");
    strcat (sql, "v.\"ValueIndex\" AS 'ValueIndex',\n");
    strcat (sql, "v.\"ByteValue\" AS 'ByteValue', ");
    strcat (sql, "v.\"StringValue\" AS 'StringValue', ");
    strcat (sql, "v.\"NumValue\" AS 'NumValue', ");
    strcat (sql, "v.\"NumValueBis\" AS 'NumValueBis',\n");
    strcat (sql, "v.\"DoubleValue\" AS 'DoubleValue', ");
    strcat (sql, "v.\"HumanReadable\" AS 'HumanReadable'\n");
    strcat (sql,
	    "FROM \"ExifPhoto\" AS p, \"ExifTags\" AS t, \"ExifValues\" AS v\n");
    strcat (sql,
	    "WHERE t.\"PhotoId\" = p.\"PhotoId\" AND v.\"PhotoId\" = t.\"PhotoId\" AND v.\"TagId\" = t.\"TagId\"");
    ret = sqlite3_exec (handle, sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK)
      {
	  printf ("CREATE VIEW ExifMetadata error: %s\n", err_msg);
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

/* all right, it's empty: proceeding to initialize */
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
    fprintf (stderr, "\n\nusage: exif_loader ARGLIST\n");
    fprintf (stderr,
	     "==============================================================\n");
    fprintf (stderr,
	     "-h or --help                    print this help message\n");
    fprintf (stderr,
	     "-d or --db-path    pathname     the SpatiaLite db path\n");
    fprintf (stderr,
	     "-D or --dir        dir_path     the DIR path containing EXIF files\n");
    fprintf (stderr, "-f or --file-path  file_name    a single EXIF file\n\n");
    fprintf (stderr, "you can specify the following options as well\n");
    fprintf (stderr, "--any-exif         *default*\n");
    fprintf (stderr, "--gps-exif-only\n\n");
    fprintf (stderr, "--metadata         *default*\n");
    fprintf (stderr, "--no-metadata\n\n");
}

int
main (int argc, char *argv[])
{
/* the MAIN function simply perform arguments checking */
    int i;
    int next_arg = ARG_NONE;
    char *path = NULL;
    char *dir_path = NULL;
    char *file_path = NULL;
    int gps_only = 0;
    int metadata = 1;
    int error = 0;
    sqlite3 *handle;
    int ret;
    int cnt = 0;
    void *cache;
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
		  case ARG_DIR:
		      dir_path = argv[i];
		      break;
		  case ARG_FILE:
		      file_path = argv[i];
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
	  if (strcasecmp (argv[i], "--dir-path") == 0)
	    {
		next_arg = ARG_DIR;
		continue;
	    }
	  if (strcmp (argv[i], "-D") == 0)
	    {
		next_arg = ARG_DIR;
		continue;
	    }
	  if (strcasecmp (argv[i], "--file-path") == 0)
	    {
		next_arg = ARG_FILE;
		continue;
	    }
	  if (strcmp (argv[i], "-f") == 0)
	    {
		next_arg = ARG_FILE;
		continue;
	    }
	  if (strcasecmp (argv[i], "--any-exif") == 0)
	    {
		gps_only = 0;
		continue;
	    }
	  if (strcasecmp (argv[i], "--gps-exif-only") == 0)
	    {
		gps_only = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--metatada") == 0)
	    {
		metadata = 1;
		continue;
	    }
	  if (strcasecmp (argv[i], "--no-metadata") == 0)
	    {
		metadata = 0;
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
    if (!dir_path && !file_path)
      {
	  fprintf (stderr,
		   "did you forget setting the --dir_path OR --file_path argument ?\n");
	  error = 1;
      }
    if (dir_path && file_path)
      {
	  fprintf (stderr,
		   "--dir_path AND --file_path argument are mutually exclusive\n");
	  error = 1;
      }
    if (error)
      {
	  do_help ();
	  return -1;
      }
/* trying to connect the SpatiaLite DB  */
    ret =
	sqlite3_open_v2 (path, &handle,
			 SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (ret != SQLITE_OK)
      {
	  fprintf (stderr, "cannot open '%s': %s\n", path,
		   sqlite3_errmsg (handle));
	  sqlite3_close (handle);
	  return -1;
      }
    cache = spatialite_alloc_connection ();
    spatialite_init_ex (handle, cache, 0);
    spatialite_autocreate (handle);
    if (!checkExifTables (handle))
      {
	  fprintf (stderr,
		   "An EXIF table is already defined, but has incompatible columns\n\nSorry ...\n");
	  sqlite3_close (handle);
	  return -1;
      }
    if (dir_path)
	cnt = load_dir (handle, dir_path, gps_only, metadata);
    else
	cnt = load_file (handle, file_path, gps_only, metadata);
    ret = sqlite3_close (handle);
    if (ret != SQLITE_OK)
	fprintf (stderr, "sqlite3_close() error: %s\n",
		 sqlite3_errmsg (handle));
    spatialite_cleanup_ex (cache);
    if (cnt)
	fprintf (stderr,
		 "\n\n***   %d EXIF photo%s successfully inserted into the DB\n",
		 cnt, (cnt > 1) ? "s where" : " was");
spatialite_shutdown();
    return 0;
}
