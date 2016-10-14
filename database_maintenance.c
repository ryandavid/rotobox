#include "database.h"
#include "database_maintenance.h"

/*
static bool verify_db_table_airports() {
    bool success = true;

    database_execute_query("PRAGMA table_info('airports');");
    while (database_fetch_row() == true) {
        // Column 1 - Name
        // Column 2 - Type
        bool foundMatch = false;
        for(size_t i = 0; i < sizeof(database_airports_table) / sizeof(database_airports_table[0]); i++) {
            if((strncmp(database_column_text(1), database_airports_table[i][0], strlen(database_airports_table[i][0])) == 0) &&
               (strncmp(database_column_text(2), database_airports_table[i][1], strlen(database_airports_table[i][1])) == 0)) {
                foundMatch = true;
                break;
            }
        }

        if(foundMatch == false) {
            fprintf(stdout, "Failed match for %s\n", database_column_text(1));
        }
        success &= foundMatch;
    }

    return success;
}
*/

static bool create_db_table_airports() {
    const char* create_table_query = "CREATE TABLE airports(" \
        "id VARCHAR(32) PRIMARY KEY UNIQUE, " \
        "landing_facility_type VARCHAR(32), " \
        "location_identifier VARCHAR(8), " \
        "effective_date INTEGER, " \
        "region_code VARCHAR(4), " \
        "faa_district_code VARCHAR(4), " \
        "state_code VARCHAR(4), " \
        "state_name VARCHAR(32), " \
        "county_name VARCHAR(32), " \
        "county_state VARCHAR(4), " \
        "city_name VARCHAR(64), " \
        "facility_name VARCHAR(64), " \
        "ownership_type VARCHAR(4), " \
        "facility_use VARCHAR(4), " \
        "owner_name VARCHAR(32), " \
        "owner_address VARCHAR(128), " \
        "owner_address2 VARCHAR(64), " \
        "owner_phone VARCHAR(16), " \
        "manager_name VARCHAR(64), " \
        "manager_address VARCHAR(128), " \
        "manager_address2 VARCHAR(64), " \
        "manager_phone VARCHAR(16), " \
        "location_surveyed BOOLEAN, " \
        "elevation FLOAT, " \
        "elevation_surveyed BOOLEAN, " \
        "magnetic_variation FLOAT, " \
        "magnetic_epoch_year INTEGER, " \
        "tpa INTEGER, " \
        "sectional VARCHAR(32), " \
        "associated_city_distance INTEGER, " \
        "associated_city_direction VARCHAR(4), " \
        "land_covered FLOAT, " \
        "boundary_artcc_id VARCHAR(4), " \
        "boundary_artcc_computer_id VARCHAR(4), " \
        "boundary_artcc_name VARCHAR(32), " \
        "responsible_artcc_id VARCHAR(4), " \
        "responsible_artcc_computer_id VARCHAR(4), " \
        "responsible_artcc_name VARCHAR(32), " \
        "fss_on_site BOOLEAN, " \
        "fss_id VARCHAR(16), " \
        "fss_name VARCHAR(32), " \
        "fss_admin_phone VARCHAR(16), " \
        "fss_pilot_phone VARCHAR(16), " \
        "alt_fss_id VARCHAR(16), " \
        "alt_fss_name VARCHAR(32), " \
        "alt_fss_pilot_phone VARCHAR(16), " \
        "notam_facility_id VARCHAR(32), " \
        "notam_d_avail BOOLEAN, " \
        "activation_date INTEGER, " \
        "status_code VARCHAR(4), " \
        "arff_certification_type VARCHAR(32), " \
        "agreements_code VARCHAR(16), " \
        "airspace_analysis_det VARCHAR(16), " \
        "entry_for_customs BOOLEAN, " \
        "landing_rights BOOLEAN, " \
        "mil_civ_joint_use BOOLEAN, " \
        "mil_landing_rights BOOLEAN, " \
        "inspection_method VARCHAR(4), " \
        "inspection_agency VARCHAR(4), " \
        "inspection_date INTEGER, " \
        "information_request_date VARCHAR(4), " \
        "fuel_types_avail VARCHAR(64), " \
        "airframe_repair_avail VARCHAR(16), " \
        "powerplant_repair_avail VARCHAR(16), " \
        "oxygen_avail VARCHAR(16), " \
        "bulk_oxygen_avail VARCHAR(16), " \
        "lighting_schedule VARCHAR(16), " \
        "beacon_schedule VARCHAR(16), " \
        "tower_onsite BOOLEAN, " \
        "unicom_freq FLOAT, " \
        "ctaf_freq FLOAT, " \
        "segmented_circle VARCHAR(4), " \
        "beacon_lens_color VARCHAR(4), " \
        "non_commerical_ldg_fee BOOLEAN, " \
        "medical_use BOOLEAN, " \
        "num_se_aircraft INTEGER, " \
        "num_me_aircraft INTEGER, " \
        "num_jet_aircraft INTEGER, " \
        "num_helicopters INTEGER, " \
        "num_gliders INTEGER, " \
        "num_mil_aircraft INTEGER, " \
        "num_ultralight INTEGER, " \
        "ops_commerical INTEGER, " \
        "ops_commuter INTEGER, " \
        "ops_air_taxi INTEGER, " \
        "ops_general_local INTEGER, " \
        "ops_general_iternant INTEGER, " \
        "ops_military INTEGER, " \
        "operations_date INTEGER, " \
        "position_source VARCHAR(16), " \
        "position_date INTEGER, " \
        "elevation_source VARCHAR(16), " \
        "elevation_date INTEGER, " \
        "contract_fuel_avail BOOLEAN, " \
        "transient_storage_facilities VARCHAR(16), " \
        "other_services VARCHAR(128), " \
        "wind_indicator VARCHAR(16), " \
        "icao_identifier VARCHAR(8), " \
        "attendance_schedule VARCHAR(128));";

    const char* geometry_query = "SELECT AddGeometryColumn(" \
        "'airports', 'location', 4326, 'POINT', 'XY');";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS airports;");
    database_execute_query(create_table_query);
    database_execute_query(geometry_query);
    database_execute_query("END TRANSACTION;");
    return true;
}

static bool create_db_table_airspaces() {
    const char* create_table_query = "CREATE TABLE airspaces(" \
        "id INTEGER PRIMARY KEY UNIQUE, " \
        "name VARCHAR(128), " \
        "airspace VARCHAR(128), " \
        "low_alt VARCHAR(16), " \
        "high_alt VARCHAR(16), " \
        "type VARCHAR(16))";

    const char* geometry_query = "SELECT AddGeometryColumn(" \
        "'airspaces', 'geometry', 4326, 'POLYGON', 'XY');";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS airspaces;");
    database_execute_query(create_table_query);
    database_execute_query(geometry_query);
    database_execute_query("END TRANSACTION;");
    return true;
}

static bool create_db_table_awos() {
    const char* create_table_query = "CREATE TABLE awos(" \
        "id VARCHAR(64) PRIMARY KEY UNIQUE, " \
        "type VARCHAR(16), " \
        "commissioning BOOLEAN, " \
        "commissioning_date INTEGER, " \
        "associated_with_naviad BOOLEAN, " \
        "elevation FLOAT, " \
        "surveyed BOOLEAN, " \
        "frequency FLOAT, " \
        "frequency2 FLOAT, " \
        "phone_number VARCHAR(16), " \
        "phone_number2 VARCHAR(16), " \
        "associated_facility VARCHAR(16), " \
        "city VARCHAR(64), " \
        "state VARCHAR(4), " \
        "effective_date INTEGER);";

    const char* geometry_query = "SELECT AddGeometryColumn(" \
        "'awos', 'location', 4326, 'POINT', 'XY');";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS awos;");
    database_execute_query(create_table_query);
    database_execute_query(geometry_query);
    database_execute_query("END TRANSACTION;");
    return true;
}

static bool create_db_table_charts() {
    const char* create_table_query = "CREATE TABLE charts(" \
        "id INTEGER PRIMARY KEY UNIQUE, " \
        "chart_type VARCHAR(32), " \
        "chart_name VARCHAR(32), " \
        "current_date INTEGER, " \
        "current_number INTEGER, " \
        "current_url VARCHAR(256), " \
        "next_date INTEGER, " \
        "next_number INTEGER, " \
        "next_url VARCHAR(256), " \
        "to_download BOOLEAN);";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS charts;");
    database_execute_query(create_table_query);
    database_execute_query("END TRANSACTION;");
    return true;
}

static bool create_db_table_runways() {
    const char* create_table_query = "CREATE TABLE runways(" \
        "id INTEGER PRIMARY KEY UNIQUE, " \
        "airport_id VARCHAR(32), " \
        "name VARCHAR(16), " \
        "length INTEGER, " \
        "width INTEGER, " \
        "surface_type VARCHAR(16), " \
        "surface_treatment VARCHAR(16), " \
        "pavement_classification VARCHAR(16), " \
        "lights_intensity VARCHAR(8), " \
        "base_id VARCHAR(8), " \
        "base_true_hdg INTEGER, " \
        "base_ils_type VARCHAR(16), " \
        "base_rh_traffic BOOLEAN, " \
        "base_markings VARCHAR(16), " \
        "base_markings_condition VARCHAR(4), " \
        "base_elevation FLOAT, " \
        "base_threshold_height INTEGER, " \
        "base_glide_angle FLOAT, " \
        "base_disp_threshold_elevation FLOAT, " \
        "base_disp_threshold_distance FLOAT, " \
        "base_touchdown_elevation FLOAT, " \
        "base_glideslope_indicators VARCHAR(8), " \
        "base_visual_range_equip VARCHAR(8), " \
        "base_visual_range_avail BOOLEAN, " \
        "base_app_lighting VARCHAR(16), " \
        "base_reil_avail BOOLEAN, " \
        "base_center_lights_avail BOOLEAN, " \
        "base_touchdown_lights_avail BOOLEAN, " \
        "base_obstacle_description VARCHAR(16), " \
        "base_obstacle_lighting VARCHAR(4), " \
        "base_obstacle_category VARCHAR(8), " \
        "base_obstacle_slope INTEGER, " \
        "base_obstacle_height INTEGER, " \
        "base_obstacle_distance INTEGER, " \
        "base_obstacle_offset INTEGER, " \
        "recip_id VARCHAR(8), " \
        "recip_true_hdg INTEGER, " \
        "recip_ils_type VARCHAR(16), " \
        "recip_rh_traffic BOOLEAN, " \
        "recip_markings VARCHAR(16), " \
        "recip_markings_condition VARCHAR(4), " \
        "recip_elevation FLOAT, " \
        "recip_threshold_height INTEGER, " \
        "recip_glide_angle FLOAT, " \
        "recip_disp_threshold_elevation FLOAT, " \
        "recip_disp_threshold_distance FLOAT, " \
        "recip_touchdown_elevation FLOAT, " \
        "recip_glideslope_indicators VARCHAR(8), " \
        "recip_visual_range_equip VARCHAR(8), " \
        "recip_visual_range_avail BOOLEAN, " \
        "recip_app_lighting VARCHAR(16), " \
        "recip_reil_avail BOOLEAN, " \
        "recip_center_lights_avail BOOLEAN, " \
        "recip_touchdown_lights_avail BOOLEAN, " \
        "recip_obstacle_description VARCHAR(16), " \
        "recip_obstacle_lighting VARCHAR(4), " \
        "recip_obstacle_category VARCHAR(8), " \
        "recip_obstacle_slope INTEGER, " \
        "recip_obstacle_height INTEGER, " \
        "recip_obstacle_distance INTEGER, " \
        "recip_obstacle_offset INTEGER, " \
        "length_source VARCHAR(16), " \
        "length_source_date INTEGER, " \
        "weight_cap_single_wheel INTEGER, " \
        "weight_cap_dual_wheel INTEGER, " \
        "weight_cap_two_dual_wheel INTEGER, " \
        "weight_cap_tandem_dual_wheel INTEGER, " \
        "base_gradient FLOAT, " \
        "base_position_source VARCHAR(16), " \
        "base_position_source_date INTEGER, " \
        "base_elevation_source VARCHAR(16), " \
        "base_elevation_source_date INTEGER, " \
        "base_disp_threshold_source VARCHAR(16), " \
        "base_disp_threshold_source_date INTEGER, " \
        "base_disp_threshold_elevation_source VARCHAR(16), " \
        "base_disp_threshold_elevation_source_date INTEGER, " \
        "base_takeoff_run INTEGER, " \
        "base_takeoff_distance INTEGER, " \
        "base_aclt_stop_distance INTEGER, " \
        "base_landing_distance INTEGER, " \
        "base_lahso_distance INTEGER, " \
        "base_intersecting_runway_id VARCHAR(16), " \
        "base_hold_short_description VARCHAR(64), " \
        "base_lahso_source VARCHAR(16), " \
        "base_lahso_source_date INTEGER, " \
        "recip_gradient FLOAT, " \
        "recip_position_source VARCHAR(16), " \
        "recip_position_source_date INTEGER, " \
        "recip_elevation_source VARCHAR(16), " \
        "recip_elevation_source_date INTEGER, " \
        "recip_disp_threshold_source VARCHAR(16), " \
        "recip_disp_threshold_source_date INTEGER, " \
        "recip_disp_threshold_elevation_source VARCHAR(16), " \
        "recip_disp_threshold_elevation_source_date INTEGER, " \
        "recip_takeoff_run INTEGER, " \
        "recip_takeoff_distance INTEGER, " \
        "recip_aclt_stop_distance INTEGER, " \
        "recip_landing_distance INTEGER, " \
        "recip_lahso_distance INTEGER, " \
        "recip_intersecting_runway_id VARCHAR(16), " \
        "recip_hold_short_description VARCHAR(64), " \
        "recip_lahso_source VARCHAR(16), " \
        "recip_lahso_source_date INTEGER);";

    const char* geometry_query = \
        "SELECT AddGeometryColumn('runways', 'base_location', 4326, 'POINT', 'XY'); " \
        "SELECT AddGeometryColumn('runways', 'base_disp_threshold_location', 4326, 'POINT', 'XY'); " \
        "SELECT AddGeometryColumn('runways', 'recip_location', 4326, 'POINT', 'XY'); " \
        "SELECT AddGeometryColumn('runways', 'recip_disp_threshold_location', 4326, 'POINT', 'XY'); " \
        "SELECT AddGeometryColumn('runways', 'base_lahso_position', 4326, 'POINT', 'XY'); " \
        "SELECT AddGeometryColumn('runways', 'recip_lahso_position', 4326, 'POINT', 'XY');";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS runways;");
    database_execute_query(create_table_query);
    database_execute_query(geometry_query);
    database_execute_query("END TRANSACTION;");

    return true;
}

static bool create_db_table_tpp() {
    const char* create_table_query = "CREATE TABLE tpp(" \
        "id INTEGER PRIMARY KEY UNIQUE, " \
        "airport_id VARCHAR(32), " \
        "chart_code VARCHAR(32), " \
        "chart_name VARCHAR(64), " \
        "filename VARCHAR(64), " \
        "url VARCHAR(256));";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS tpp;");
    database_execute_query(create_table_query);
    database_execute_query("END TRANSACTION;");

    return true;
}

static bool create_db_table_uat_text() {
    const char* create_table_query = "CREATE TABLE uat_text(" \
        "id INTEGER PRIMARY KEY UNIQUE, " \
        "received TEXT, " \
        "valid TEXT, " \
        "type VARCHAR(16), " \
        "location VARCHAR(16), " \
        "report VARCHAR(512));";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS uat_text;");
    database_execute_query(create_table_query);
    database_execute_query("END TRANSACTION;");
    return true;
}

static bool create_db_table_updates() {
    const char* create_table_query = "CREATE TABLE updates(" \
        "id INTEGER PRIMARY KEY UNIQUE, " \
        "product VARCHAR(32), " \
        "cycle VARCHAR(32), " \
        "updated DATETIME DEFAULT CURRENT_TIMESTAMP);";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS updates;");
    database_execute_query(create_table_query);
    database_execute_query("END TRANSACTION;");

    return true;
}

static bool create_db_table_waypoints() {
    const char* create_table_query = "CREATE TABLE waypoints(" \
        "id VARCHAR(64) PRIMARY KEY UNIQUE, " \
        "state_name VARCHAR(16), " \
        "region_code VARCHAR(64), " \
        "previous_name VARCHAR(64), " \
        "charting_info VARCHAR(64), " \
        "to_be_published BOOLEAN, " \
        "fix_use VARCHAR(32), " \
        "nas_identifier VARCHAR(16), " \
        "high_artcc VARCHAR(16), " \
        "low_artcc VARCHAR(16), " \
        "country_name VARCHAR(16), " \
        "sua_atcaa BOOLEAN, " \
        "remark VARCHAR(128), " \
        "depicted_chart VARCHAR(32), " \
        "mls_component VARCHAR(32), " \
        "radar_component VARCHAR(32), " \
        "pitch BOOLEAN, " \
        "catch BOOLEAN, " \
        "type VARCHAR(8));";

    const char* geometry_query = "SELECT AddGeometryColumn(" \
        "'waypoints', 'location', 4326, 'POINT', 'XY');";

    database_execute_query("BEGIN TRANSACTION;");
    database_execute_query("DROP TABLE IF EXISTS waypoints;");
    database_execute_query(create_table_query);
    database_execute_query(geometry_query);
    database_execute_query("END TRANSACTION;");

    return true;
}

const char* database_table_cmds[][2] = {
    {"airports",    "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='airports';"},
    {"airspaces",   "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='airspaces';"},
    {"awos",        "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='awos';"},
    {"charts",      "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='charts';"},
    {"runways",     "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='runways';"},
    {"tpp",         "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='tpp';"},
    {"uat_text",    "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='uat_text';"},
    {"updates",     "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='updates';"},
    {"waypoints",   "SELECT count(name) FROM sqlite_master WHERE type='table' AND name='waypoints';"}
};

bool (*database_table_helpers[])(void) = {
    create_db_table_airports,
    create_db_table_airspaces,
    create_db_table_awos,
    create_db_table_charts,
    create_db_table_runways,
    create_db_table_tpp,
    create_db_table_uat_text,
    create_db_table_updates,
    create_db_table_waypoints
};


bool database_maintenance(bool rebuild) {
    fprintf(stdout, "Verifying database tables.\n");
    for(size_t i = 0; i < (sizeof(database_table_cmds) / sizeof(database_table_cmds[0])); i++) {
        bool tableValid = false;

        database_prepare(database_table_cmds[i][1]);
        if(database_fetch_row() == true) {
            tableValid = (database_column_int(0) == 1);
        }

        if(tableValid == false) {
            if(rebuild == true) {
                fprintf(stdout, "Table '%s' is borked.  Rebuilding it.\n", database_table_cmds[i][0]);
                database_table_helpers[i]();
            } else {
                fprintf(stdout, "Table '%s' is borked, but skipping rebuild.\n", database_table_cmds[i][0]);
            }
        }
    }
    return true;
}
