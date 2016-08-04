#include "database.h"

static sqlite3 *db;
static sqlite3_stmt *stmt;

static void database_trace(void *arg1, const char* string) {
    fprintf(stdout, "[SQL] %s\n", string);
}


bool database_init() {
    bool success = false;

    // TODO: Become more robust in DB filepath.
    if(sqlite3_open(DATABASE_FILEPATH, &db) == SQLITE_OK){
        success = true;
        // Dump the assembled queries to stdout.
        sqlite3_trace(db, database_trace, NULL);
    }

    return success;
}

bool database_close() {
    sqlite3_close(db);
    return true;
}

bool database_fetch_row() {
    return (sqlite3_step(stmt) == SQLITE_ROW);
}

void database_finish_query() {
    sqlite3_finalize(stmt);
}

int database_num_columns() {
    return sqlite3_column_count(stmt);
}

enum database_column_type_t database_column_type(int i) {
    return sqlite3_column_type(stmt, i);
}

const char* database_column_name(int i) {
    return sqlite3_column_name(stmt, i);
}

const char* database_column_text(int i) {
    return sqlite3_column_text(stmt, i);
}

int database_column_int(int i) {
    return sqlite3_column_int(stmt, i);
}

double database_column_double(int i) {
    return sqlite3_column_double(stmt, i);
}

void database_search_radio_by_airport_id(int airport_id) {
    const char *query = "SELECT radio.* " \
                        "FROM radio " \
                        "JOIN airports ON radio.airport_faa_id = airports.faa_id " \
                        "WHERE airports.id = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, airport_id);
}

void database_search_airport_by_id(int airport_id) {
    const char *query = "SELECT * " \
                        "FROM airports " \
                        "WHERE id = ? " \
                        "LIMIT 1;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, airport_id);
}

void database_search_runways_by_airport_id(int airport_id) {
    const char *query = "SELECT runways.* " \
                        "FROM runways JOIN airports ON runways.airport_faa_id = airports.faa_id " \
                        "WHERE airports.id = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, airport_id);
}

void database_search_charts_by_airport_id(int airport_id) {
    const char *query = "SELECT tpp.filename, tpp.chart_name "\
                        "FROM tpp JOIN airports ON airports.designator = tpp.airport_id "\
                        "WHERE airports.id = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, airport_id);
}

void database_search_airports_within_window(float latMin, float latMax,
                                            float lonMin, float lonMax) {
    const char *query = "SELECT * FROM airports " \
                        "WHERE latitude BETWEEN ? AND ? " \
                        "AND longitude BETWEEN ? AND ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, latMin);
    sqlite3_bind_double(stmt, 2, latMax);
    sqlite3_bind_double(stmt, 3, lonMin);
    sqlite3_bind_double(stmt, 4, lonMax);
}

void database_search_airports_by_name(const char* name) {
    const char *query = "SELECT * " \
                        "FROM airports " \
                        "WHERE (icao_name LIKE '%' || ? || '%') " \
                        "OR (name LIKE '%' || ? || '%') " \
                        "OR (designator LIKE '%' || ? || '%');";
    fprintf(stdout, "Name = %s, len = %d\n", wildcardName, strlen(wildcardName));

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, strlen(name), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, name, strlen(name), SQLITE_STATIC);
}

