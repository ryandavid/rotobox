#include "database.h"
#include "database_maintenance.h"

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

    spatialite_initialize();
    spatialite_cache = spatialite_alloc_connection();
    spatialite_init_ex(db, spatialite_cache, 0);

    // We must make sure to call this at least once for new DBs.  Subsequent calling is harmless.
    fprintf(stdout, "Setting up table metadata. This may take a minute...\n");
    database_execute_query("SELECT InitSpatialMetaData(1);");

    // Verify all the rotobox tables are present.
    database_maintenance(true);

    // Clear out any old received METAR's, TAF's, etc.
    database_empty_old_uat_text(UAT_TEXT_MAX_AGE_HOURS);

    fprintf(stdout, "DB Path: %s\n", DATABASE_FILEPATH);
    fprintf(stdout, "SQLite version: %s\n", sqlite3_libversion());
    fprintf(stdout, "SpatiaLite version: %s\n", spatialite_version());
    fprintf(stdout, "RasterLite2 version: %s\n", rl2_version());
    fprintf(stdout, "GEOS version: %s\n", GEOSversion());
    fprintf(stdout, "PROJ version: %s\n", pj_get_release());
    fprintf(stdout, "\n\n");

    return success;
}

bool database_close() {
    spatialite_cleanup_ex(spatialite_cache);
    sqlite3_close(db);
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

const unsigned char* database_column_text(int i) {
    return sqlite3_column_text(stmt, i);
}

int database_column_int(int i) {
    return sqlite3_column_int(stmt, i);
}

double database_column_double(int i) {
    return sqlite3_column_double(stmt, i);
}

void database_alter_table(const char* table_name, const char* column_name, const char* column_type) {
    const char* query = "ALTER TABLE ? ADD COLUMN ? ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, table_name, strlen(table_name), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, column_name, strlen(column_name), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, column_type, strlen(column_type), SQLITE_STATIC);
    database_finish_query();
}

void database_search_radio_by_airport_id(const char* airport_id) {
    const char *query = "SELECT airports.ctaf_freq, airports.unicom_freq, " \
                        "       awos.frequency awos_freq, awos.phone_number awos_phone " \
                        "FROM airports " \
                        "LEFT JOIN awos ON awos.associated_facility = airports.id " \
                        "WHERE airports.id = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_id, strlen(airport_id), SQLITE_STATIC);
}

void database_search_airport_by_id(const char* airport_id) {
    // There's gotta be a better way....
    const char *query = "SELECT " \
                        "X(location) as longitude, Y(location) as latitude, " \
                        "landing_facility_type, location_identifier, effective_date, region_code, " \
                        "faa_district_code, state_code, state_name, county_name, county_state, " \
                        "city_name, facility_name, ownership_type, facility_use, owner_name, " \
                        "owner_address, owner_address2, owner_phone, manager_name, manager_address, " \
                        "manager_address2, manager_phone, location_surveyed, elevation, elevation_surveyed, " \
                        "magnetic_variation, magnetic_epoch_year, tpa, sectional, associated_city_distance, " \
                        "associated_city_direction, land_covered, boundary_artcc_id, " \
                        "boundary_artcc_computer_id, boundary_artcc_name, responsible_artcc_id, " \
                        "responsible_artcc_computer_id, responsible_artcc_name, fss_on_site, " \
                        "fss_id, fss_name, fss_admin_phone, fss_pilot_phone, alt_fss_id, alt_fss_name, " \
                        "alt_fss_pilot_phone, notam_facility_id, notam_d_avail, activation_date, " \
                        "status_code, arff_certification_type, agreements_code, airspace_analysis_det, " \
                        "entry_for_customs, landing_rights, mil_civ_joint_use, mil_landing_rights, " \
                        "inspection_method, inspection_agency, inspection_date, information_request_date, " \
                        "fuel_types_avail, airframe_repair_avail, powerplant_repair_avail, oxygen_avail, " \
                        "bulk_oxygen_avail, lighting_schedule, beacon_schedule, tower_onsite, segmented_circle, " \
                        "beacon_lens_color, non_commerical_ldg_fee, medical_use, num_se_aircraft, num_me_aircraft, " \
                        "num_jet_aircraft, num_helicopters, num_gliders, num_mil_aircraft, num_ultralight, " \
                        "ops_commerical, ops_commuter, ops_air_taxi, ops_general_local, ops_general_iternant, " \
                        "ops_military, operations_date, position_source, position_date, elevation_source, " \
                        "elevation_date, contract_fuel_avail, transient_storage_facilities, other_services, " \
                        "wind_indicator, icao_identifier, attendance_schedule " \
                        "FROM airports " \
                        "WHERE id = ? " \
                        "LIMIT 1;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_id, strlen(airport_id), SQLITE_STATIC);
}

void database_search_runways_by_airport_id(const char* airport_id) {
    const char *query = "SELECT " \
                        "name, length, width, surface_type, surface_treatment, pavement_classification, " \
                        "lights_intensity, base_id, base_true_hdg, base_ils_type, base_rh_traffic, " \
                        "base_markings, base_markings_condition, X(base_location) base_longitude, " \
                        "Y(base_location) base_latitude, base_threshold_height, base_glide_angle, " \
                        "X(base_disp_threshold_location) base_disp_threshold_longitude, " \
                        "Y(base_disp_threshold_location) base_disp_threshold_latitude, " \
                        "base_disp_threshold_elevation, base_disp_threshold_distance, " \
                        "base_touchdown_elevation, base_glideslope_indicators, base_visual_range_equip, " \
                        "base_visual_range_avail, base_app_lighting, base_reil_avail, base_center_lights_avail, " \
                        "base_touchdown_lights_avail, base_obstacle_description, base_obstacle_lighting, " \
                        "base_obstacle_category, base_obstacle_slope, base_obstacle_height, " \
                        "base_obstacle_distance, base_obstacle_offset, recip_id, recip_true_hdg, " \
                        "recip_ils_type, recip_rh_traffic, recip_markings, recip_markings_condition, " \
                        "X(recip_location) recip_longitude, Y(recip_location) recip_latitude, recip_elevation, " \
                        "recip_threshold_height, recip_glide_angle, X(recip_disp_threshold_location) " \
                        "recip_disp_threshold_longitude, Y(recip_disp_threshold_location) " \
                        "recip_disp_threshold_latitude, recip_disp_threshold_elevation, " \
                        "recip_disp_threshold_distance, recip_touchdown_elevation, recip_glideslope_indicators, " \
                        "recip_visual_range_equip, recip_visual_range_avail, recip_app_lighting, " \
                        "recip_reil_avail, recip_center_lights_avail, recip_touchdown_lights_avail, " \
                        "recip_obstacle_description, recip_obstacle_lighting, recip_obstacle_category, " \
                        "recip_obstacle_slope, recip_obstacle_height, recip_obstacle_distance, " \
                        "recip_obstacle_offset, length_source, length_source_date, weight_cap_single_wheel, " \
                        "weight_cap_dual_wheel, weight_cap_two_dual_wheel, weight_cap_tandem_dual_wheel, " \
                        "base_gradient, base_position_source, base_position_source_date, base_elevation_source, " \
                        "base_elevation_source_date, base_disp_threshold_source, base_disp_threshold_source_date, " \
                        "base_disp_threshold_elevation_source, base_disp_threshold_elevation_source_date, " \
                        "base_takeoff_run, base_takeoff_distance, base_aclt_stop_distance, base_landing_distance, " \
                        "base_lahso_distance, base_intersecting_runway_id, base_hold_short_description, " \
                        "X(base_lahso_position) base_lahso_longitude, Y(base_lahso_position) " \
                        "base_lahso_latitude, base_lahso_source, base_lahso_source_date, recip_gradient, " \
                        "recip_position_source, recip_position_source_date, recip_elevation_source, " \
                        "recip_elevation_source_date, recip_disp_threshold_source, recip_disp_threshold_source_date, " \
                        "recip_disp_threshold_elevation_source, recip_disp_threshold_elevation_source_date, "\
                        "recip_takeoff_run, recip_takeoff_distance, recip_aclt_stop_distance, " \
                        "recip_landing_distance, recip_lahso_distance, recip_intersecting_runway_id, " \
                        "recip_hold_short_description, X(recip_lahso_position) recip_lahso_longitude, " \
                        "Y(recip_lahso_position) recip_lahso_latitude, recip_lahso_source, recip_lahso_source_date "
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
    const char *query = "SELECT id, facility_name, location_identifier, X(location) longitude, " \
                        "Y(location) latitude " \
                        "FROM airports " \
                        "WHERE MbrWithin(location, BuildMbr(?, ?, ?, ?, 4326))";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, lonMin);
    sqlite3_bind_double(stmt, 2, latMin);
    sqlite3_bind_double(stmt, 3, lonMax);
    sqlite3_bind_double(stmt, 4, latMax);
}

void database_search_airports_by_name(const char* name) {
    const char *query = "SELECT id, facility_name, location_identifier, X(location) as longitude, "\
                        "Y(location) as latitude " \
                        "FROM airports " \
                        "WHERE (icao_identifier LIKE ?) " \
                        "OR (facility_name LIKE '%' || ? || '%') " \
                        "OR (location_identifier LIKE ?);";

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
    const char *query = "SELECT id, facility_name, location_identifier, "\
                        "ST_Distance(location, MakePoint(?, ?, 4326), 1) AS distance " \
                        "FROM airports " \
                        "ORDER BY distance ASC " \
                        "LIMIT 10";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, lon);
    sqlite3_bind_double(stmt, 2, lat);
}

void database_get_available_faa_charts() {
    const char *query = "SELECT * FROM charts;";
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
}

void database_set_faa_chart_download_flag(int chart_id, bool to_download) {
    const char *query = "UPDATE charts SET to_download = ? WHERE id = ?;";
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, to_download);
    sqlite3_bind_int(stmt, 2, chart_id);
}

void database_insert_uat_text_product(char* receivedTime, char* productType, char* productTime,
                                      char* location, char* report) {
    int32_t row_id = -1;

    // First find out if we are are updating an existing record.
    const char* query = "SELECT id " \
                        "FROM uat_text " \
                        "WHERE type = ? " \
                        "AND valid = ? " \
                        "AND location = ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, productType, strlen(productType), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, productTime, strlen(productTime), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, location, strlen(location), SQLITE_STATIC);
    if(database_fetch_row() == true) {
        row_id = database_column_int(0);
    }

    database_execute_query("BEGIN TRANSACTION;");

    if(row_id == -1) {
        query = "INSERT INTO uat_text(received, valid, type, location, report) " \
                "VALUES(?, ?, ?, ?, ?);";

        sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, receivedTime, strlen(receivedTime), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, productTime, strlen(productTime), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, productType, strlen(productType), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, location, strlen(location), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, report, strlen(report), SQLITE_STATIC);
    } else {
        query = "UPDATE uat_text " \
                "SET received = ?, valid = ?, type = ?, location = ?, report = ? " \
                "WHERE id = ? " \
                "LIMIT 1;";

        sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, receivedTime, strlen(receivedTime), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, productTime, strlen(productTime), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, productType, strlen(productType), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, location, strlen(location), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, report, strlen(report), SQLITE_STATIC);
        sqlite3_bind_int(stmt, 6, row_id);
    }

    // Roundabout way of calling sqlite3_step
    database_fetch_row();

    database_execute_query("END TRANSACTION;");
}

void database_get_recent_winds() {
    const char *query = "SELECT received, valid, location, report, " \
                        "(julianday('now') - julianday(received)) * 24 * 60 age_minutes "\
                        "FROM uat_text " \
                        "WHERE type = 'WINDS' " \
                        "ORDER BY location, valid DESC;";
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
}

void database_get_metar_by_airport_id(const char* airport_id) {
    const char *query = "SELECT ut.received, ut.valid, ut.location, ut.report, " \
                        "(julianday('now') - julianday(ut.received)) * 24 * 60 age_minutes " \
                        "FROM uat_text ut " \
                        "JOIN airports a ON ut.location = a.icao_identifier " \
                        "WHERE a.id = ? " \
                        "AND ut.type IN ('METAR', 'SPECI') " \
                        "ORDER BY ut.valid DESC;";
    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_id, strlen(airport_id), SQLITE_STATIC);
}

void database_empty_old_uat_text(uint16_t age_hours) {
    database_execute_query("BEGIN TRANSACTION;");

    const char* query = "DELETE FROM uat_text " \
                        "WHERE (julianday('now') - julianday(valid)) * 24 > ?;";

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, age_hours);
    // Roundabout way of calling sqlite3_step
    database_fetch_row();

    database_execute_query("END TRANSACTION;");
}
