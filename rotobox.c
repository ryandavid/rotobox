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

#include "dump978.h"
#include "dump1090.h"
#include "fec.h"
#include "gdl90.h"
#include "mongoose.h"
#include "uat_decode.h"
#include "rotobox.h"
#include "sqlite3.h"

volatile bool exitRequested = false;
rtlsdr_dev_t *device978, *device1090;
struct gps_data_t rx_gps_data;
sqlite3 *db;
char path_to_db[256];

static const char *s_http_port = "80";
static struct mg_serve_http_opts s_http_server_opts;

// Define an event handler function
static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    mg_serve_http(nc, (struct http_message *) p, s_http_server_opts);
  }
}

static void sqlite_trace(void *arg1, const char* string) {
    fprintf(stdout, "[SQL] %s\n", string);
}

static const bool get_argument_value(struct mg_str *args, const char *name, char *value) {
    char scratch[1024];
    size_t length = sizeof(scratch);

    // Assume we use the entire buffer, and reduce it if we can.
    if (args->len < length) {
        length = args->len;
    }

    // Copy it over and then null terminate it.
    memcpy(&scratch[0], args->p, length);
    scratch[length] = 0x00;

    char *argName = NULL;
    char *pos = strtok(&scratch[0], "=");
    while (pos != NULL) {
        // Copy over the argument name.
        argName = pos;

        // Snag the value. TODO make this more robust
        pos = strtok(NULL, "&");

        // Check this is the argument we are interested in.
        if(strcmp(name, argName) == 0){
            // TODO: Check the copy length
            memcpy(value, pos, strlen(pos));
            value[strlen(pos)] = 0x00;
            break;
        }
        // Set up for the next argument
        pos = strtok(NULL, "=");
    }

    return (value != NULL);
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
    for (int i = 0; i < rx_gps_data.satellites_visible; i++) {
        mg_printf(nc, \
            "    \"%d\": {\n" \
            "        \"snr\": %f,\n" \
            "        \"used\": %d,\n" \
            "        \"elevation\": %d,\n" \
            "        \"azimuth\": %d\n" \
            "    },\n", \
            rx_gps_data.skyview[i].PRN, \
            rx_gps_data.skyview[i].ss, \
            rx_gps_data.skyview[i].used, \
            rx_gps_data.skyview[i].elevation, \
            rx_gps_data.skyview[i].azimuth);
    }
    mg_printf(nc, "    \"num_satellites\": %d,\n" \
                  "    \"num_satellites_used\": %d\n", \
                  rx_gps_data.satellites_visible, \
                  rx_gps_data.satellites_used);
    mg_printf(nc, "}\n");

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

static void api_airport_name_search(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    struct http_message *message = (struct http_message *)ev_data;
    sqlite3_stmt *stmt;
    char airport_name[256];
    
    const char *query = "SELECT * FROM airports WHERE icao_name LIKE ?;";
    // TODO: Check for success.
    get_argument_value(&message->query_string, "name", &airport_name[0]);

    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n{\n");

    sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, airport_name, strlen(airport_name), SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        size_t numColumns = sqlite3_column_count(stmt);
        mg_printf(nc, "    [\n");
        for (size_t i = 0; i < numColumns; i++) {
            mg_printf(nc,
                      "        \"%s\" = \"%s\",\n",
                      sqlite3_column_name(stmt, i),
                      sqlite3_column_text(stmt, i));
        }
        mg_printf(nc, "    ],\n");
    }

    mg_printf(nc, "}\n");
    sqlite3_finalize(stmt);
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
    

    // Signal Handlers
    signal(SIGINT, handle_sigint);
    gdl90_crcInit();

    // Init GPSD
    if(gps_open("localhost", "2947", &rx_gps_data) != 0) {
        fprintf(stdout, "ERROR: Could not connect to GPSD\n");
    } else {
        gps_stream(&rx_gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
        gpsd_available = true;
    }

    // Init sqlite3
    // TODO: Become more robust in DB filepath
    if(sqlite3_open("./airports/airport_db.sqlite", &db) != SQLITE_OK){
        fprintf(stdout, "ERROR: Could not open SQLite DB\n");
    }
    sqlite3_trace(db, sqlite_trace, NULL);

    // Init Webserver
    mg_mgr_init(&mgr, NULL);  // Initialize event manager object
    nc = mg_bind(&mgr, s_http_port, ev_handler);

    if(nc != NULL) {
        // Set up HTTP server parameters
        mg_set_protocol_http_websocket(nc);
        s_http_server_opts.document_root = "./wwwroot";
        mg_register_http_endpoint(nc, "/api/location", api_location);
        mg_register_http_endpoint(nc, "/api/satellites", api_satellites);
        mg_register_http_endpoint(nc, "/api/airports/search", api_airport_name_search);
    } else {
        fprintf(stdout, "ERROR: Could not bind to port %s\n", s_http_port);
    }

    /*
    // Init 978MHz receiver
    device978 = init_SDR("0978", RECEIVER_CENTER_FREQ_HZ_978, RECEIVER_SAMPLING_HZ_978);
    if(device978 != NULL) {
        init_dump978();
        if(pthread_create(&thread_978, NULL, dump978_worker, NULL) != 0) {
            fprintf(stdout, "Failed to create 978MHz thread!\n");
        }
    }

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

    sqlite3_close(db);
    
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

void dump978_callback(uint64_t timestamp, uint8_t *buffer, int receiveErrors, frame_type_t type) {
    fprintf(stdout, "\nts=%llu, rs=%d\n", timestamp, receiveErrors);

    if(type == FRAME_TYPE_ADSB) {
        struct uat_adsb_mdb mdb;
        uat_decode_adsb_mdb(buffer, &mdb);
        uat_display_adsb_mdb(&mdb, stdout);
    } else if(type == FRAME_TYPE_UAT) {
        struct uat_uplink_mdb mdb;
        uat_decode_uplink_mdb(buffer, &mdb);
        uat_display_uplink_mdb(&mdb, stdout);
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
