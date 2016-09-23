#ifndef API_H_
#define API_H_

#include <gps.h>
#include <pthread.h>

#include "database.h"
#include "mongoose.h"

void api_location(struct mg_connection *nc, int ev, void *ev_data);
void api_satellites(struct mg_connection *nc, int ev, void *ev_data);
void api_airport_name_search(struct mg_connection *nc, int ev, void *ev_data);
void api_airport_window_search(struct mg_connection *nc, int ev, void *ev_data);
void api_airport_id_search(struct mg_connection *nc, int ev, void *ev_data);
void api_airport_runway_search(struct mg_connection *nc, int ev, void *ev_data);
void api_airport_radio_search(struct mg_connection *nc, int ev, void *ev_data);
void api_airport_diagram_search(struct mg_connection *nc, int ev, void *ev_data);
void api_airport_find_nearest(struct mg_connection *nc, int ev, void *ev_data);
void api_available_airspace_shapefiles(struct mg_connection *nc, int ev, void *ev_data);
void api_airspace_geojson_by_class(struct mg_connection *nc, int ev, void *ev_data);
void api_available_faa_charts(struct mg_connection *nc, int ev, void *ev_data);
void api_set_faa_chart_download_flag(struct mg_connection *nc, int ev, void *ev_data);
void api_uat_get_winds(struct mg_connection *nc, int ev, void *ev_data);
void api_metar_by_airport_id(struct mg_connection *nc, int ev, void *ev_data);
void api_get_traffic(struct mg_connection *nc, int ev, void *ev_data);

#endif  // API_H_
