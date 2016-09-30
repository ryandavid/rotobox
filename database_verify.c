#include "database_verify.h"

static bool verify_db_table_airports() {
	fprintf(stdout, "Verifying 'airports' table\n");
	return false;
}

static bool verify_db_table_airspaces() {
	fprintf(stdout, "Verifying 'airspaces' table\n");
	return false;
}

static bool verify_db_table_awos() {
	fprintf(stdout, "Verifying 'awos' table\n");
	return false;
}

static bool verify_db_table_charts() {
	fprintf(stdout, "Verifying 'charts' table\n");
	return false;
}

static bool verify_db_table_runways() {
	fprintf(stdout, "Verifying 'runways' table\n");
	return false;
}

static bool verify_db_table_tpp() {
	fprintf(stdout, "Verifying 'tpp' table\n");
	return false;
}

static bool verify_db_table_uat_text() {
	fprintf(stdout, "Verifying 'uat_text' table\n");
	return false;
}

static bool verify_db_table_updates() {
	fprintf(stdout, "Verifying 'updates' table\n");
	return false;
}

static bool verify_db_table_waypoints() {
	fprintf(stdout, "Verifying 'waypoints' table\n");
	return false;
}


static bool create_db_table_airports() {
	return false;
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

bool (*database_table_helpers[][2])(void) = {
	{verify_db_table_airports,		create_db_table_airports},
	{verify_db_table_airspaces,		create_db_table_airspaces},
	{verify_db_table_awos, 			create_db_table_awos},
	{verify_db_table_charts, 		create_db_table_charts},
	{verify_db_table_runways, 		create_db_table_runways},
	{verify_db_table_tpp, 			create_db_table_tpp},
	{verify_db_table_uat_text, 		create_db_table_uat_text},
	{verify_db_table_updates, 		create_db_table_updates},
	{verify_db_table_waypoints, 	create_db_table_waypoints}
};


bool database_verify(bool rebuild) {
	for(size_t i = 0; i < (sizeof(database_table_helpers) / sizeof(database_table_helpers[0])); i++) {

		if(database_table_helpers[i][0]() == false) {
			if(rebuild == true) {
				database_table_helpers[i][1]();
			}
		} else {
			fprintf(stdout, "ERROR: Table is shot!\n");
		}
	}

	return true;
}
