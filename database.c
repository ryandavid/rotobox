#include "database.h"

static sqlite3 *db;
static sqlite3_stmt *stmt;
void *spatialite_cache;

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

    spatialite_cache = spatialite_alloc_connection();
    spatialite_init_ex(db, spatialite_cache, 0);

    // We must make sure to call this at least once for new DBs.  Subsequent calling is harmless.
    database_execute_query("SELECT InitSpatialMetaData();");

    fprintf(stdout, "DB Path: %s\n", DATABASE_FILEPATH);
    fprintf(stdout, "SQLite version: %s\n", sqlite3_libversion());
    fprintf(stdout, "SpatiaLite version: %s\n", spatialite_version());
    fprintf(stdout, "\n\n");

    return success;
}

bool database_close() {
    sqlite3_close(db);
    spatialite_cleanup_ex(spatialite_cache);
    spatialite_shutdown();
    return true;
}

bool database_fetch_row() {
    return (sqlite3_step(stmt) == SQLITE_ROW);
}

void database_finish_query() {
    sqlite3_finalize(stmt);
}

void database_execute_query(const char * query) {
    sqlite3_exec(db, query, NULL, NULL, NULL);
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

void database_search_radio_by_airport_id(const char* airport_id) {
    const char *query = "SELECT radio.* " \
                        "FROM radio " \
                        "WHERE airport_id = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_id, strlen(airport_id), SQLITE_STATIC);
}

void database_search_airport_by_id(const char* airport_id) {
    // There's gotta be a better way....
    const char *query = "SELECT " \
                        "served_city, lighting_schedule, magnetic_variation, name, private_use, " \
                        "sectional_chart, activated, beacon_lighting_schedule, designator, " \
                        "traffic_control_tower_on_airport, segmented_circle_marker_on_airport, " \
                        "attendance_schedule, wind_direction_indicator, marker_lens_color, " \
                        "field_elevation, remarks, type, id, control_type, icao_name, " \
                        "X(geometry) as longitude, Y(geometry) as latitude " \
                        "FROM airports " \
                        "WHERE id = ? " \
                        "LIMIT 1;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_id, strlen(airport_id), SQLITE_STATIC);
}

void database_search_runways_by_airport_id(const char* airport_id) {
    const char *query = "SELECT * " \
                        "FROM runways " \
                        "WHERE airport_id = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_id, strlen(airport_id), SQLITE_STATIC);
}

void database_search_charts_by_airport_id(const char* airport_id) {
    const char *query = "SELECT filename, chart_name "\
                        "FROM tpp "\
                        "WHERE airport_id = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_id, strlen(airport_id), SQLITE_STATIC);
}

void database_search_airports_within_window(float latMin, float latMax,
                                            float lonMin, float lonMax) {
    const char *query = "SELECT id, name, designator, designator, X(geometry) as longitude, " \
                        "Y(geometry) as latitude " \
                        "FROM airports " \
                        "WHERE MbrWithin(geometry, BuildMbr(?, ?, ?, ?, 4326))";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, lonMin);
    sqlite3_bind_double(stmt, 2, latMin);
    sqlite3_bind_double(stmt, 3, lonMax);
    sqlite3_bind_double(stmt, 4, latMax);
}

void database_search_airports_by_name(const char* name) {
    const char *query = "SELECT id, name, designator, X(geometry) as longitude, "\
                        "Y(geometry) as latitude " \
                        "FROM airports " \
                        "WHERE (icao_name LIKE ?) " \
                        "OR (name LIKE '%' || ? || '%') " \
                        "OR (designator LIKE ?);";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, name, strlen(name), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, name, strlen(name), SQLITE_STATIC);
}

void database_available_airspace_shapefiles() {
    const char *query = "SELECT id, name, airspace, type, low_alt, high_alt FROM airspaces;";
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
}

void database_get_airspace_geojson_by_class(const char* class) {
    const char *query = "SELECT id, name, airspace, type, low_alt, high_alt, " \
                        "AsGeoJSON(geometry) as geometry " \
                        "FROM airspaces " \
                        "WHERE type LIKE ?;";
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, class, strlen(class), SQLITE_STATIC);
}

void database_find_nearest_airports(float lat, float lon) {
    const char *query = "SELECT id, name, designator, "\
                        "ST_Distance(geometry, MakePoint(?, ?, 4326), 1) AS distance " \
                        "FROM airports " \
                        "ORDER BY distance ASC " \
                        "LIMIT 10";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, lon);
    sqlite3_bind_double(stmt, 2, lat);
}

