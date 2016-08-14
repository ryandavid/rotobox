#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gps.h>
#include <rtl-sdr.h>

#include "database.h"
#include "dump1090.h"
#include "gdl90.h"
#include "mongoose.h"
#include "rotobox.h"

volatile bool exitRequested = false;
rtlsdr_dev_t *device978, *device1090;
struct gps_data_t rx_gps_data;

//uat_uplink_mdb_ll_t *uplink_mdb_head = NULL;

static const char *s_http_port = "80";
static struct mg_serve_http_opts s_http_server_opts;

// Define an event handler function
static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    mg_serve_http(nc, (struct http_message *) p, s_http_server_opts);
  }
}

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


static void api_location(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;
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

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

static void api_satellites(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n{\n");
    mg_printf(nc, "\"satellites\": [\n");
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

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

static void generic_api_db_dump(struct mg_connection *nc){
    bool first = true;
    size_t numColumns = database_num_columns();
    bool isGeo = false;
    const char * geoColumnName = "geometry";

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
            if(strcmp(database_column_name(i), &geoColumnName[0]) == 0) {
                isGeo = true;
            } else {
                isGeo = false;
            }

            switch (database_column_type(i)) {
                case(TYPE_INTEGER):
                    mg_printf(nc, "%d", database_column_int(i));
                    break;

                case(TYPE_FLOAT):
                    mg_printf(nc, "%f", database_column_double(i));
                    break;

                case(TYPE_BLOB):
                case(TYPE_NULL):
                case(TYPE_TEXT):
                default:
                    if (isGeo == true) {
                        mg_printf(nc, "%s", database_column_text(i));
                    } else {
                        mg_printf(nc, "\"%s\"", database_column_text(i));
                    }
            }

            if (i < numColumns - 1) {
                mg_printf(nc, "%s", ",\n");
            } else {
                mg_printf(nc, "%s", "\n");
            }
        }
        mg_printf(nc, "    }");
    }
    mg_printf(nc, "\n]\n");
}

static void api_send_empty_result(struct mg_connection *nc) {
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n{}\n");
}

static void api_airport_name_search(struct mg_connection *nc, int ev, void *ev_data) {
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

static void api_airport_window_search(struct mg_connection *nc, int ev, void *ev_data) {
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

static void api_airport_id_search(struct mg_connection *nc, int ev, void *ev_data) {
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


static void api_airport_runway_search(struct mg_connection *nc, int ev, void *ev_data) {
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

static void api_airport_radio_search(struct mg_connection *nc, int ev, void *ev_data) {
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

static void api_airport_diagram_search(struct mg_connection *nc, int ev, void *ev_data) {
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

static void api_available_airspace_shapefiles(struct mg_connection *nc, int ev, void *ev_data) {
    database_available_airspace_shapefiles();
    generic_api_db_dump(nc);
    database_finish_query();
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

static void api_airspace_geojson_by_class(struct mg_connection *nc, int ev, void *ev_data) {
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

void handle_sigint() {
    fprintf(stdout, "Caught SIGINT!\n");
    exitRequested = true;
}

int main(int argc, char **argv) {
    bool gpsd_available = false;
    pthread_t thread_978 = NULL, thread_1090 = NULL;
    struct mg_mgr mgr;
    struct mg_connection *nc;
    char gpsd_address[GPSD_ADDRESS_BUFFER_SIZE];

    // By default, use 'localhost' for the GPSD address
    snprintf(&gpsd_address[0], GPSD_ADDRESS_BUFFER_SIZE, "%s", "localhost");
    char c;
    while ((c = getopt(argc, argv, "a:")) != -1) {
        switch (c) {
            case('a'):
                snprintf(&gpsd_address[0], GPSD_ADDRESS_BUFFER_SIZE, "%s", optarg);
                break;

            default:
                fprintf(stdout, "Unknown flag %c\n", c);
        }
    }

    // Signal Handlers
    signal(SIGINT, handle_sigint);
    gdl90_crcInit();

    // Init GPSD
    fprintf(stdout, "Connecting to GPSD via %s\n", &gpsd_address[0]);
    if(gps_open(gpsd_address, GPSD_DEFAULT_PORT, &rx_gps_data) != 0) {
        fprintf(stdout, "ERROR: Could not connect to GPSD\n");
    } else {
        gps_stream(&rx_gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
        gpsd_available = true;
    }

    // Init sqlite3
    if(database_init() == false) {
        fprintf(stdout, "ERROR: Could not open SQLite DB\n");
    }

    // Init Webserver
    mg_mgr_init(&mgr, NULL);  // Initialize event manager object
    nc = mg_bind(&mgr, s_http_port, ev_handler);

    if(nc != NULL) {
        // Set up HTTP server parameters
        mg_set_protocol_http_websocket(nc);
        s_http_server_opts.document_root = "./wwwroot";
        mg_register_http_endpoint(nc, "/api/location", api_location);
        mg_register_http_endpoint(nc, "/api/satellites", api_satellites);
        mg_register_http_endpoint(nc, "/api/airports/search_by_name", api_airport_name_search);
        mg_register_http_endpoint(nc, "/api/airports/search_by_window", api_airport_window_search);
        mg_register_http_endpoint(nc, "/api/airports/search_by_id", api_airport_id_search);
        mg_register_http_endpoint(nc, "/api/airports/runways", api_airport_runway_search);
        mg_register_http_endpoint(nc, "/api/airports/radio", api_airport_radio_search);
        mg_register_http_endpoint(nc, "/api/airports/diagram", api_airport_diagram_search);
        mg_register_http_endpoint(nc, "/api/airspace", api_available_airspace_shapefiles);
        mg_register_http_endpoint(nc, "/api/airspace/geojson", api_airspace_geojson_by_class);
    } else {
        fprintf(stdout, "ERROR: Could not bind to port %s\n", s_http_port);
    }


    // Init 978MHz receiver
    device978 = init_SDR("0978", RECEIVER_CENTER_FREQ_HZ_978, RECEIVER_SAMPLING_HZ_978);
    if(device978 != NULL) {
        init_dump978();
        if(pthread_create(&thread_978, NULL, dump978_worker, NULL) != 0) {
            fprintf(stdout, "Failed to create 978MHz thread!\n");
        }
    }

    /*
    // Init 1090MHz receiver
    device1090 = init_SDR("1090", RECEIVER_CENTER_FREQ_HZ_1090, RECEIVER_SAMPLING_HZ_1090);
    if(device1090 != NULL) {
        init_dump1090();
        if(pthread_create(&thread_1090, NULL, dump1090_worker, NULL) != 0) {
            fprintf(stdout, "Failed to create 1090MHz thread!\n");
        }
    }
    */

    // Wait until SIGINT
    while(exitRequested == false) {
        mg_mgr_poll(&mgr, 500);
        if(gpsd_available == true) {
            gps_read(&rx_gps_data);
        }
    }

    if(thread_978 != NULL) {
        pthread_join(thread_978, NULL);
    }

    if(device978 != NULL){
        rtlsdr_close(device978);
        cleanup_dump978();
    }
    
    if(thread_1090 != NULL) {
        pthread_join(thread_1090, NULL);
    }

    if(device1090 != NULL) {
        rtlsdr_close(device1090);
        cleanup_dump1090();
    }

    if(gpsd_available == true) {
        gps_stream(&rx_gps_data, WATCH_DISABLE, NULL);
        gps_close(&rx_gps_data);
    }

    mg_mgr_free(&mgr);

    database_close();
    
    fprintf(stdout, "Exiting!\n");
}


rtlsdr_dev_t *init_SDR(const char *serialNumber, long centerFrequency, int samplingFreq) {
    rtlsdr_dev_t *device = NULL;
    char manufacturer[256], name[256], serial[256];

    int index = rtlsdr_get_index_by_serial(serialNumber);
    if(index >= 0) {
        rtlsdr_get_device_usb_strings(index, &manufacturer[0], &name[0], &serial[0]);
        fprintf(stdout, "Opening device %d: %s %s, S/N: %s\n", index, manufacturer, name, serial);

        if (rtlsdr_open(&device, index) == 0) {
            fprintf(stdout, "Successfully opened device!\n");

            rtlsdr_set_tuner_gain_mode(device, 1);  // 1 indicates manual mode
            rtlsdr_set_tuner_gain(device, RECEIVER_GAIN_TENTHS_DB);  // Tenths of a dB
            rtlsdr_set_center_freq(device, centerFrequency);
            rtlsdr_set_sample_rate(device, samplingFreq);
            rtlsdr_reset_buffer(device);
        } else {
            fprintf(stdout, "ERROR: Found device index %d but could not open it!\n", index);
        }
    } else if(index == -1) {
        fprintf(stdout, "ERROR: Invalid serial number '%s' given!\n", serialNumber);
    } else if(index == -2) {
        fprintf(stdout, "ERROR: No RTL-SDR devices available!\n");
    } else if(index == -3) {
        fprintf(stdout, "ERROR: Could not find device with serial '%s'!\n", serialNumber);
    } else {
        fprintf(stdout, "ERROR: Unknown error occurred when opening RTL-SDR device!\n");
    }

    return device;
}

// A clone of the dump978 read_from_stdin function, but using rtlsdr_read_sync instead
void *dump978_worker() {
    char dump978_buffer[65536 * 2];
    int numBytesRead = 0;
    int bytesUsed = 0;
    int offset = 0;

    while((device978 != NULL) & (exitRequested == false)){
        // Hacky way to get the number of bytes in multiples of LIBRTLSDR_MIN_READ_SIZE
        int readLength = ((sizeof(dump978_buffer) - bytesUsed) / LIBRTLSDR_MIN_READ_SIZE) * LIBRTLSDR_MIN_READ_SIZE;
        int readResult = rtlsdr_read_sync(device978, &dump978_buffer[0] + bytesUsed, readLength, &numBytesRead);

        if(readResult != 0){
            // TODO(rdavid): Do something smart to try and recover
            fprintf(stdout, "ERROR: 978MHz SDR read returned %d\n", readResult);
        } else {
            convert_to_phi((uint16_t*)(dump978_buffer + (bytesUsed & ~1)), ((bytesUsed & 1) + numBytesRead) / 2);
            bytesUsed += numBytesRead;
            int processed = process_buffer((uint16_t*)dump978_buffer, bytesUsed / 2, offset, dump978_callback);
            bytesUsed -= processed * 2;
            offset += processed;
            if (bytesUsed > 0) {
                memmove(dump978_buffer, dump978_buffer + (processed * 2), bytesUsed);
            }
        }

    }

    pthread_exit(NULL);
}

/*
static uat_uplink_mdb_ll_t * upsert_uplink_mdb_frame(struct uat_uplink_mdb *mdb){
    int count = 0;

    uat_uplink_mdb_ll_t *tail = uplink_mdb_head;
    uat_uplink_mdb_ll_t *following = NULL;

    // Special case if this is the first frame.
    if(uplink_mdb_head == NULL) {
        uplink_mdb_head = malloc(sizeof(uat_uplink_mdb_ll_t));
        tail = uplink_mdb_head;
        following = NULL;
        count += 1;

    } else {
        while(tail->next != NULL){
            if(tail->mdb.)
            tail = tail->next;
            count += 1;
        }

        tail->next = malloc(sizeof(uat_uplink_mdb_ll_t));
        tail->next->next = NULL;
    }



    fprintf(stdout, "Count = %d\n", count);
    return tail;
}*/


void dump978_callback(uint64_t timestamp, uint8_t *buffer, int receiveErrors, frame_type_t type) {
    fprintf(stdout, "\nts=%llu, rs=%d\n", timestamp, receiveErrors);

    if(type == FRAME_TYPE_ADSB) {
        struct uat_adsb_mdb mdb;
        uat_decode_adsb_mdb(buffer, &mdb);
        uat_display_adsb_mdb(&mdb, stdout);
    } else if(type == FRAME_TYPE_UAT) {
        struct uat_uplink_mdb mdb;
        uat_decode_uplink_mdb(buffer, &mdb);

        for(uint32_t i = 0; i < mdb.num_info_frames; i++) {
            fprintf(stdout, "Rx Type: %d (%d bytes)\n", mdb.info_frames[i].type, mdb.info_frames[i].length);
            if(mdb.info_frames[i].type == 0){
                fprintf(stdout, "Product ID: %d\n", mdb.info_frames[i].fisb.product_id);

                
            }
        }
        //uat_uplink_mdb_ll_t *mdb_ll = upsert_uplink_mdb_frame(&mdb);
        
        //uat_display_uplink_mdb(&(mdb_ll->mdb), stdout);
    } else {
        fprintf(stdout, "ERROR: Received unknown message type %d!\n", type);
    }
}

void init_dump978() {
    // Init from dump978
    make_atan2_table();
    init_fec();
}

void cleanup_dump978() {
    //TODO(rdavid): Actually clean up after dump978
}

// A mashup of dump1090
void *dump1090_worker() {
    char dump1090_buffer[16*16384];
    int numBytesRead = 0;
    int bytesUsed = 0;
    //int offset = 0;
    struct mag_buf *outbuf;

    outbuf = &Modes.mag_buffers[0];

    while((device1090 != NULL) & (exitRequested == false)){
        int readLength = ((sizeof(dump1090_buffer) - bytesUsed) / LIBRTLSDR_MIN_READ_SIZE) * LIBRTLSDR_MIN_READ_SIZE;
        int readResult = rtlsdr_read_sync(device1090, &dump1090_buffer[0] + bytesUsed, readLength, &numBytesRead);

        if(readResult != 0){
            // TODO(rdavid): Do something smart to try and recover
            fprintf(stdout, "ERROR: 1090MHz SDR read returned %d\n", readResult);
        } else {
            outbuf->length = numBytesRead/2;
            Modes.converter_function(&dump1090_buffer[0], &outbuf->data[0], outbuf->length, Modes.converter_state, 0);

            demodulate2400(outbuf, dump1090_callback);
        }
    }
    pthread_exit(NULL);
}

void dump1090_callback(struct modesMessage *mm) {
    displayModesMessage(mm);
}

// Rip off of dump1090's Modes init, but more hardcoded to what we want for rotobox
void init_dump1090() {
    // Default everything to zero/NULL
    memset(&Modes, 0, sizeof(Modes));

    // Now initialise things that should not be 0/NULL to their defaults
    Modes.gain                    = MODES_MAX_GAIN;
    Modes.freq                    = RECEIVER_CENTER_FREQ_HZ_1090;
    Modes.ppm_error               = MODES_DEFAULT_PPM;
    Modes.check_crc               = 1;
    Modes.net_heartbeat_interval  = MODES_NET_HEARTBEAT_INTERVAL;
    Modes.interactive_display_ttl = MODES_INTERACTIVE_DISPLAY_TTL;
    Modes.html_dir                = HTMLPATH;
    Modes.json_interval           = 1000;
    Modes.json_location_accuracy  = 1;
    Modes.maxRange                = 1852 * 300; // 300NM default max range
    Modes.sample_rate             = RECEIVER_SAMPLING_HZ_1090;
    Modes.trailing_samples = (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS + 16) * 1e-6 * Modes.sample_rate;
    Modes.oversample              = 1;
    Modes.mode_ac                 = 0;
    Modes.nfix_crc                = MODES_MAX_BITERRORS;
    Modes.phase_enhance           = 1;
    Modes.quiet                   = 1;

    Modes.maglut     = (uint16_t *) malloc(sizeof(uint16_t) * 256 * 256);
    Modes.log10lut   = (uint16_t *) malloc(sizeof(uint16_t) * 256 * 256);

    Modes.fUserLat = 0.0;
    Modes.fUserLon = 0.0;

    if ((Modes.mag_buffers[0].data = calloc(MODES_MAG_BUF_SAMPLES+Modes.trailing_samples, sizeof(uint16_t))) == NULL) {
        fprintf(stderr, "Out of memory allocating magnitude buffer.\n");
        exit(1);
    }

    Modes.mag_buffers[0].length = 0;
    Modes.mag_buffers[0].dropped = 0;
    Modes.mag_buffers[0].sampleTimestamp = 0;


    // compute UC8 magnitude lookup table
    for (int i = 0; i <= 255; i++) {
        for (int q = 0; q <= 255; q++) {
            float fI, fQ, magsq;

            fI = (i - 127.5) / 127.5;
            fQ = (q - 127.5) / 127.5;
            magsq = fI * fI + fQ * fQ;
            if (magsq > 1)
                magsq = 1;

            Modes.maglut[le16toh((i*256)+q)] = (uint16_t) round(sqrtf(magsq) * 65535.0);
        }
    }


    // Prepare the log10 lookup table: 100log10(x)
    Modes.log10lut[0] = 0; // poorly defined..
    for (int i = 1; i <= 65535; i++) {
        Modes.log10lut[i] = (uint16_t) round(100.0 * log10(i));
    }

    // Prepare error correction tables
    modesChecksumInit(Modes.nfix_crc);
    icaoFilterInit();

    Modes.input_format = INPUT_UC8;

    Modes.converter_function = init_converter(Modes.input_format,
                                                  Modes.sample_rate,
                                                  Modes.dc_filter,
                                                  Modes.measure_noise, /* total power is interesting if we want noise */
                                                  &Modes.converter_state);
}

void cleanup_dump1090() {
    //TODO(rdavid): Actually clean up after modes
}
