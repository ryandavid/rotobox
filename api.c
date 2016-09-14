#include "api.h"

static bool get_argument_text(struct mg_str *args, const char *name, char *buf, int buflen) {
    return mg_get_http_var(args, name, buf, buflen) > 0;
}

static int get_argument_int(struct mg_str *args, const char *name, int *value) {
    bool success = false;
    char buffer[64];

    if(get_argument_text(args, name, &buffer[0], sizeof(buffer)) == true) {
        *value = atoi(buffer);
        success = true;
    }

    return success;
}

static int get_argument_float(struct mg_str *args, const char *name, float *value) {
    bool success = false;
    char buffer[64];

    if(get_argument_text(args, name, &buffer[0], sizeof(buffer)) == true) {
        *value = atof(buffer);
        success = true;
    }

    return success;
}

static void api_send_empty_result(struct mg_connection *nc) {
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n{}\n");
}

static void generic_api_db_dump(struct mg_connection *nc) {
    bool first = true;
    size_t numColumns = database_num_columns();

    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n[\n");
    while (database_fetch_row() == true) {
        if(!first) {
            mg_printf(nc, ",\n");
        } else {
            first = false;
        }

        mg_printf(nc, "    {\n");
        for (size_t i = 0; i < numColumns; i++) {
            mg_printf(nc, "        \"%s\": ", database_column_name(i));

            switch (database_column_type(i)) {
                case(TYPE_INTEGER):
                    mg_printf(nc, "%d", database_column_int(i));
                    break;

                case(TYPE_FLOAT):
                    mg_printf(nc, "%f", database_column_double(i));
                    break;

                case(TYPE_NULL):
                    mg_printf(nc, "null");
                    break;

                case(TYPE_BLOB):
                case(TYPE_TEXT):
                default:
                    // Some hackery to figure out if this column contains geometry (ie, WKT).
                    if(strcmp(database_column_name(i), "geometry") == 0) {
                        mg_printf(nc, "%s", database_column_text(i));
                    } else {
                        mg_printf(nc, "\"%s\"", database_column_text(i));
                    }
            }

            // If this is the last column, then don't put a comma!
            if (i < numColumns - 1) {
                mg_printf(nc, ",\n");
            } else {
                mg_printf(nc, "\n");
            }
        }
        mg_printf(nc, "    }");
    }
    mg_printf(nc, "\n]\n");
}


void api_location(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;

    pthread_mutex_lock(&gps_mutex);
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n" \
        "{\n" \
        "    \"status\": %d,\n" \
        "    \"timestamp\": %f,\n" \
        "    \"mode\": %d,\n" \
        "    \"sats_used\": %d,\n" \
        "    \"latitude\": %f,\n" \
        "    \"longitude\": %f,\n" \
        "    \"altitude\": %f,\n" \
        "    \"track\": %f,\n" \
        "    \"speed\": %f,\n" \
        "    \"climb\": %f,\n" \
        "    \"uncertainty_pos\": [\n" \
        "         %f,\n" \
        "         %f,\n" \
        "         %f\n" \
        "     ],\n" \
        "    \"uncertainty_speed\": %f\n" \
        "}\n", \
        rx_gps_data.status,
        rx_gps_data.fix.time, \
        rx_gps_data.fix.mode, \
        rx_gps_data.satellites_used, \
        rx_gps_data.fix.latitude, \
        rx_gps_data.fix.longitude, \
        rx_gps_data.fix.altitude, \
        rx_gps_data.fix.track, \
        rx_gps_data.fix.speed, \
        rx_gps_data.fix.climb, \
        rx_gps_data.fix.epx, \
        rx_gps_data.fix.epy, \
        rx_gps_data.fix.epv, \
        rx_gps_data.fix.eps);

    pthread_mutex_unlock(&gps_mutex);

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_satellites(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n{\n");
    mg_printf(nc, "\"satellites\": [\n");

    pthread_mutex_lock(&gps_mutex);
    for (int i = 0; i < rx_gps_data.satellites_visible; i++) {
        mg_printf(nc, \
            "    {\n" \
            "        \"prn\": %d,\n" \
            "        \"snr\": %f,\n" \
            "        \"used\": %d,\n" \
            "        \"elevation\": %d,\n" \
            "        \"azimuth\": %d\n" \
            "    }", \
            rx_gps_data.skyview[i].PRN, \
            rx_gps_data.skyview[i].ss, \
            rx_gps_data.skyview[i].used, \
            rx_gps_data.skyview[i].elevation, \
            rx_gps_data.skyview[i].azimuth);
        if(i != rx_gps_data.satellites_visible - 1){
            mg_printf(nc, ",");
        }
        mg_printf(nc, "\n");
    }
    mg_printf(nc, "],\n");
    mg_printf(nc, "    \"num_satellites\": %d,\n" \
                  "    \"num_satellites_used\": %d\n", \
                  rx_gps_data.satellites_visible, \
                  rx_gps_data.satellites_used);
    mg_printf(nc, "}\n");

    pthread_mutex_unlock(&gps_mutex);

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_name_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char name[256];

    if(get_argument_text(&message->query_string, "name", &name[0], sizeof(name)) == true){
        database_search_airports_by_name(name);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'name'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_window_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    float latMin, latMax, lonMin, lonMax;
    
    if(get_argument_float(&message->query_string, "latMin", &latMin) &&
       get_argument_float(&message->query_string, "latMax", &latMax) &&
       get_argument_float(&message->query_string, "lonMin", &lonMin) &&
       get_argument_float(&message->query_string, "lonMax", &lonMax)) {
        database_search_airports_within_window(latMin, latMax, lonMin, lonMax);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find latMin, latMax, lonMin, or lonMax");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_id_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_airport_by_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}


void api_airport_runway_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_runways_by_airport_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_radio_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];
    
    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_radio_by_airport_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_diagram_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_charts_by_airport_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_find_nearest(struct mg_connection *nc, int ev, void *ev_data) {
    // TODO: Make sure that GPSD is actually up and running.
    database_find_nearest_airports(rx_gps_data.fix.latitude, rx_gps_data.fix.longitude);
    generic_api_db_dump(nc);
    database_finish_query();
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_available_airspace_shapefiles(struct mg_connection *nc, int ev, void *ev_data) {
    database_available_airspace_shapefiles();
    generic_api_db_dump(nc);
    database_finish_query();
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airspace_geojson_by_class(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char class[16];

    if(get_argument_text(&message->query_string, "class", &class[0], sizeof(class)) == true) {
        database_get_airspace_geojson_by_class(&class[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_available_faa_charts(struct mg_connection *nc, int ev, void *ev_data) {
    database_get_available_faa_charts();
    generic_api_db_dump(nc);
    database_finish_query();
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_set_faa_chart_download_flag(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    int chart_id;
    int to_download;

    if((get_argument_int(&message->query_string, "id", &chart_id) == true) &&
       (get_argument_int(&message->query_string, "download", &to_download) == true)) {
        database_set_faa_chart_download_flag(chart_id, (bool)(to_download == 1));
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_uat_get_winds(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;
    database_get_recent_winds();
    generic_api_db_dump(nc);
    database_finish_query();

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_metar_by_airport_id(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_get_metar_by_airport_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}
