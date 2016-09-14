#ifndef DATABASE_H_
#define DATABASE_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>
#include <spatialite.h>
#include <spatialite/gaiageo.h>

#define DATABASE_FILEPATH       "./rotobox.sqlite"
#define DATABASE_WILDCARD       '%'

enum database_column_type_t {
    TYPE_INTEGER    = SQLITE_INTEGER,
    TYPE_FLOAT      = SQLITE_FLOAT,
    TYPE_BLOB       = SQLITE_BLOB,
    TYPE_NULL       = SQLITE_NULL,
    TYPE_TEXT       = SQLITE3_TEXT
};

bool database_init();
bool database_close();

bool database_fetch_row();
void database_finish_query();
void database_execute_query(const char * query);

int database_num_columns();
enum database_column_type_t database_column_type(int i);
const char* database_column_name(int i);
const unsigned char* database_column_text(int i);
int database_column_int(int i);
double database_column_double(int i);

void database_search_airport_by_id(const char* airport_id);
void database_search_radio_by_airport_id(const char* airport_id);
void database_search_runways_by_airport_id(const char* airport_id);
void database_search_charts_by_airport_id(const char* airport_id);
void database_search_airports_within_window(float latMin, float latMax, float lonMin, float lonMax);
void database_search_airports_by_name(const char* name);
void database_available_airspace_shapefiles();
void database_get_airspace_geojson_by_class(const char* class);
void database_find_nearest_airports(float lat, float lon);
void database_get_available_faa_charts();
void database_set_faa_chart_download_flag(int chart_id, bool to_download);
void database_insert_uat_text_product(char* receivedTime, char* productType, char* productTime,
                                      char* location, char* report);
void database_get_recent_winds();
void database_get_metar_by_airport_id(const char* airport_id);

#endif  // DATABASE_H_
