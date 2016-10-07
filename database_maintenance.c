#include "database.h"
#include "database_maintenance.h"

const char* database_airports_table[][2] = {
    {"id",                              "VARCHAR(32) PRIMARY KEY UNIQUE"},
    {"landing_facility_type",           "VARCHAR(32)"                   },
    {"location_identifier",             "VARCHAR(8)"                    },
    {"effective_date",                  "INTEGER"                       },
    {"region_code",                     "VARCHAR(4)"                    },
    {"faa_district_code",               "VARCHAR(4)"                    },
    {"state_code",                      "VARCHAR(4)"                    },
    {"state_name",                      "VARCHAR(32)"                   },
    {"county_name",                     "VARCHAR(32)"                   },
    {"county_state",                    "VARCHAR(4)"                    },
    {"city_name",                       "VARCHAR(64)"                   },
    {"facility_name",                   "VARCHAR(64)"                   },
    {"ownership_type",                  "VARCHAR(4)"                    },
    {"facility_use",                    "VARCHAR(4)"                    },
    {"owner_name",                      "VARCHAR(32)"                   },
    {"owner_address",                   "VARCHAR(128)"                  },
    {"owner_address2",                  "VARCHAR(64)"                   },
    {"owner_phone",                     "VARCHAR(16)"                   },
    {"manager_name",                    "VARCHAR(64)"                   },
    {"manager_address",                 "VARCHAR(128)"                  },
    {"manager_address2",                "VARCHAR(64)"                   },
    {"manager_phone",                   "VARCHAR(16)"                   },
    {"location",                        "POINT",                        },  // Spatialite Geometry
    {"location_surveyed",               "BOOLEAN"                       },
    {"elevation",                       "FLOAT"                         },
    {"elevation_surveyed",              "BOOLEAN"                       },
    {"magnetic_variation",              "FLOAT"                         },
    {"magnetic_epoch_year",             "INTEGER"                       },
    {"tpa",                             "INTEGER"                       },
    {"sectional",                       "VARCHAR(32)"                   },
    {"associated_city_distance",        "INTEGER"                       },
    {"associated_city_direction",       "VARCHAR(4)"                    },
    {"land_covered",                    "FLOAT"                         },
    {"boundary_artcc_id",               "VARCHAR(4)"                    },
    {"boundary_artcc_computer_id",      "VARCHAR(4)"                    },
    {"boundary_artcc_name",             "VARCHAR(32)"                   },
    {"responsible_artcc_id",            "VARCHAR(4)"                    },
    {"responsible_artcc_computer_id",   "VARCHAR(4)"                    },
    {"responsible_artcc_name",          "VARCHAR(32)"                   },
    {"fss_on_site",                     "BOOLEAN"                       },
    {"fss_id",                          "VARCHAR(16)"                   },
    {"fss_name",                        "VARCHAR(32)"                   },
    {"fss_admin_phone",                 "VARCHAR(16)"                   },
    {"fss_pilot_phone",                 "VARCHAR(16)"                   },
    {"alt_fss_id",                      "VARCHAR(16)"                   },
    {"alt_fss_name",                    "VARCHAR(32)"                   },
    {"alt_fss_pilot_phone",             "VARCHAR(16)"                   },
    {"notam_facility_id",               "VARCHAR(32)"                   },
    {"notam_d_avail",                   "BOOLEAN"                       },
    {"activation_date",                 "INTEGER"                       },
    {"status_code",                     "VARCHAR(4)"                    },
    {"arff_certification_type",         "VARCHAR(32)"                   },
    {"agreements_code",                 "VARCHAR(16)"                   },
    {"airspace_analysis_det",           "VARCHAR(16)"                   },
    {"entry_for_customs",               "BOOLEAN"                       },
    {"landing_rights",                  "BOOLEAN"                       },
    {"mil_civ_joint_use",               "BOOLEAN"                       },
    {"mil_landing_rights",              "BOOLEAN"                       },
    {"inspection_method",               "VARCHAR(4)"                    },
    {"inspection_agency",               "VARCHAR(4)"                    },
    {"inspection_date",                 "INTEGER"                       },
    {"information_request_date",        "VARCHAR(4)"                    },
    {"fuel_types_avail",                "VARCHAR(64)"                   },
    {"airframe_repair_avail",           "VARCHAR(16)"                   },
    {"powerplant_repair_avail",         "VARCHAR(16)"                   },
    {"oxygen_avail",                    "VARCHAR(16)"                   },
    {"bulk_oxygen_avail",               "VARCHAR(16)"                   },
    {"lighting_schedule",               "VARCHAR(16)"                   },
    {"beacon_schedule",                 "VARCHAR(16)"                   },
    {"tower_onsite",                    "BOOLEAN"                       },
    {"unicom_freq",                     "FLOAT"                         },
    {"ctaf_freq",                       "FLOAT"                         },
    {"segmented_circle",                "VARCHAR(4)"                    },
    {"beacon_lens_color",               "VARCHAR(4)"                    },
    {"non_commerical_ldg_fee",          "BOOLEAN"                       },
    {"medical_use",                     "BOOLEAN"                       },
    {"num_se_aircraft",                 "INTEGER"                       },
    {"num_me_aircraft",                 "INTEGER"                       },
    {"num_jet_aircraft",                "INTEGER"                       },
    {"num_helicopters",                 "INTEGER"                       },
    {"num_gliders",                     "INTEGER"                       },
    {"num_mil_aircraft",                "INTEGER"                       },
    {"num_ultralight",                  "INTEGER"                       },
    {"ops_commerical",                  "INTEGER"                       },
    {"ops_commuter",                    "INTEGER"                       },
    {"ops_air_taxi",                    "INTEGER"                       },
    {"ops_general_local",               "INTEGER"                       },
    {"ops_general_iternant",            "INTEGER"                       },
    {"ops_military",                    "INTEGER"                       },
    {"operations_date",                 "INTEGER"                       },
    {"position_source",                 "VARCHAR(16)"                   },
    {"position_date",                   "INTEGER"                       },
    {"elevation_source",                "VARCHAR(16)"                   },
    {"elevation_date",                  "INTEGER"                       },
    {"contract_fuel_avail",             "BOOLEAN"                       },
    {"transient_storage_facilities",    "VARCHAR(16)"                   },
    {"other_services",                  "VARCHAR(128)"                  },
    {"wind_indicator",                  "VARCHAR(16)"                   },
    {"icao_identifier",                 "VARCHAR(8)"                    },
    {"attendance_schedule",             "VARCHAR(128)"                  }
};

static bool verify_db_table_airports() {
    bool success = false;

    database_execute_query("PRAGMA table_info(airports);");

    size_t numColumns = database_num_columns();

    while (database_fetch_row() == true) {
        for (size_t i = 0; i < numColumns; i++) {
            fprintf(stdout, "%s = %s, ", database_column_name(i), database_column_text(i));
        }
        fprintf(stdout, "\n");
    }

    return success;
}

static bool verify_db_table_airspaces() {
    return false;
}

static bool verify_db_table_awos() {
    return false;
}

static bool verify_db_table_charts() {
    return false;
}

static bool verify_db_table_runways() {
    return false;
}

static bool verify_db_table_tpp() {
    return false;
}

static bool verify_db_table_uat_text() {
    return false;
}

static bool verify_db_table_updates() {
    return false;
}

static bool verify_db_table_waypoints() {
    return false;
}


static bool create_db_table_airports() {
    database_execute_query("BEGIN TRANSACTION;");

    database_execute_query("DROP TABLE IF EXISTS airports;");
    database_execute_query("CREATE TABLE airports(valid BOOL);");

    for(size_t i = 0; i < sizeof(database_airports_table) / sizeof(database_airports_table[0]); i++) {
        //if(strncmp(database_airports_table[i][1], "POINT", sizeof("POINT")) != 0) {
            database_alter_table("airports", database_airports_table[i][0], database_airports_table[i][1]);
            fprintf(stdout, "%s\n", database_airports_table[i][0]);
        //}
    }
    database_fetch_row();
    database_execute_query("END TRANSACTION;");
    return true;
}

static bool create_db_table_airspaces() {
    return false;
}

static bool create_db_table_awos() {
    return false;
}

static bool create_db_table_charts() {
    return false;
}

static bool create_db_table_runways() {
    return false;
}

static bool create_db_table_tpp() {
    return false;
}

static bool create_db_table_uat_text() {
    return false;
}

static bool create_db_table_updates() {
    return false;
}

static bool create_db_table_waypoints() {
    return false;
}

const char* database_table_names[] = {
    "airports",
    "airspaces",
    "awos",
    "charts",
    "runways",
    "tpp",
    "uat_text",
    "updates",
    "waypoints"
};

bool (*database_table_helpers[][2])(void) = {
    {verify_db_table_airports,      create_db_table_airports},
    {verify_db_table_airspaces,     create_db_table_airspaces},
    {verify_db_table_awos,          create_db_table_awos},
    {verify_db_table_charts,        create_db_table_charts},
    {verify_db_table_runways,       create_db_table_runways},
    {verify_db_table_tpp,           create_db_table_tpp},
    {verify_db_table_uat_text,      create_db_table_uat_text},
    {verify_db_table_updates,       create_db_table_updates},
    {verify_db_table_waypoints,     create_db_table_waypoints}
};


bool database_maintenance(bool rebuild) {
    for(size_t i = 0; i < (sizeof(database_table_helpers) / sizeof(database_table_helpers[0])); i++) {
        fprintf(stdout, "Verifying table '%s'\n", database_table_names[i]);
        if(database_table_helpers[i][0]() == false) {
            if(rebuild == true) {
                fprintf(stdout, "Table '%s' is borked.  Rebuilding it.\n", database_table_names[i]);
                database_table_helpers[i][1]();
            } else {
                fprintf(stdout, "Table '%s' is borked, but skipping rebuild.\n", database_table_names[i]);
            }
        } else {
            fprintf(stdout, "ERROR: Table is shot!\n");
        }
    }

    return true;
}
