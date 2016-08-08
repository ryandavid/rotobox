#ifndef DATABASE_H_
#define DATABASE_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sqlite3.h"

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

int database_num_columns();
enum database_column_type_t database_column_type(int i);
const char* database_column_name(int i);
const char* database_column_text(int i);
int database_column_int(int i);
double database_column_double(int i);

void database_search_airport_by_id(const char* airport_id);
void database_search_radio_by_airport_id(const char* airport_id);
void database_search_runways_by_airport_id(const char* airport_id);
void database_search_charts_by_airport_id(const char* airport_id);
void database_search_airports_within_window(float latMin, float latMax, float lonMin, float lonMax);
void database_search_airports_by_name(const char* name);

#endif  // DATABASE_H_
