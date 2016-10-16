#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "gps.h"
#include "rtl-sdr.h"

#include "3rd_party/dump1090/dump1090.h"
#include "3rd_party/dump978/dump978.h"
#include "3rd_party/dump978/uat_decode.h"
#include "3rd_party/mongoose/mongoose.h"

#include "api.h"
#include "database.h"
#include "download.h"
#include "gdl90.h"
#include "rotobox.h"

volatile bool exitRequested = false;
rtlsdr_dev_t *device978, *device1090;

// GPS position solution.
pthread_mutex_t gps_mutex;
struct gps_data_t rx_gps_data;

// Tracked traffic.
pthread_mutex_t uat_traffic_mutex;
struct uat_adsb_mdb tracked_traffic[MAX_TRACKED_TRAFFIC];

static struct mg_serve_http_opts s_http_server_opts;

// Define an SIGINT handler.
static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    mg_serve_http(nc, (struct http_message *) p, s_http_server_opts);
  }
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
    char update_product_name[256];

    memset(&update_product_name, 0x00, sizeof(update_product_name));

    // Clear out the tracked traffic.
    memset(&tracked_traffic, 0x00, sizeof(tracked_traffic));

    // By default, use 'localhost' for the GPSD address
    snprintf(&gpsd_address[0], GPSD_ADDRESS_BUFFER_SIZE, "%s", "localhost");
    char c;
    while ((c = getopt(argc, argv, "a:u:")) != -1) {
        switch (c) {
            case('a'):
                snprintf(&gpsd_address[0], GPSD_ADDRESS_BUFFER_SIZE, "%s", optarg);
                break;

            case('u'):
                snprintf(&update_product_name[0], sizeof(update_product_name), "%s", optarg);
                break;

            default:
                fprintf(stdout, "Unknown flag %c\n", c);
        }
    }

    // Init sqlite3
    if(database_init() == false) {
        fprintf(stdout, "ERROR: Could not open SQLite DB\n");
    }

    download_init();
    if(strlen(update_product_name) > 0) {
        download_updates(update_product_name);
        database_close();
        return 0;
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

    // Init Webserver
    mg_mgr_init(&mgr, NULL);  // Initialize event manager object
    nc = mg_bind(&mgr, WEBSERVER_PORT, ev_handler);

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
        mg_register_http_endpoint(nc, "/api/airports/nearest", api_airport_find_nearest);
        mg_register_http_endpoint(nc, "/api/airspace", api_available_airspace_shapefiles);
        mg_register_http_endpoint(nc, "/api/airspace/geojson", api_airspace_geojson_by_class);
        mg_register_http_endpoint(nc, "/api/charts", api_available_faa_charts);
        mg_register_http_endpoint(nc, "/api/charts/download", api_set_faa_chart_download_flag);
        mg_register_http_endpoint(nc, "/api/uat/winds", api_uat_get_winds);
        mg_register_http_endpoint(nc, "/api/uat/metar_by_id", api_metar_by_airport_id);
        mg_register_http_endpoint(nc, "/api/traffic", api_get_traffic);
    } else {
        fprintf(stdout, "ERROR: Could not bind to port %s\n", WEBSERVER_PORT);
    }


    // Init 978MHz receiver.
    //device978 = init_SDR("0978", RECEIVER_CENTER_FREQ_HZ_978, RECEIVER_SAMPLING_HZ_978);
    device978 = init_SDR("1090", RECEIVER_CENTER_FREQ_HZ_978, RECEIVER_SAMPLING_HZ_978);
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
            pthread_mutex_lock(&gps_mutex);
            gps_read(&rx_gps_data);
            pthread_mutex_unlock(&gps_mutex);
        }

        // Clear out old traffic.
        pthread_mutex_lock(&uat_traffic_mutex);
        pthread_mutex_unlock(&uat_traffic_mutex);
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

    download_cleanup();
    
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
        //uat_display_adsb_mdb(&mdb, stdout);
        fprintf(stdout, "Traffic: %02x\n", mdb.address);
        handle_uat_traffic(&mdb);

    } else if(type == FRAME_TYPE_UAT) {
        struct uat_uplink_mdb mdb;
        uat_decode_uplink_mdb(buffer, &mdb);

        for(uint32_t i = 0; i < mdb.num_info_frames; i++) {
            // If we received a UAT Service Status Management Message, then disregard.
            if(mdb.info_frames[i].type != UAT_TYPE_FISB_ADPU) {
                fprintf(stdout, "Type %d Service Status (%d bytes)\n", mdb.info_frames[i].type, mdb.info_frames[i].length);
                continue;
            }

            // Otherwise start handling the data product as necessary.
            fprintf(stdout, "ADPU Product %d: %s (%d bytes)\n",
                    mdb.info_frames[i].fisb.product_id,
                    get_fisb_product_name(mdb.info_frames[i].fisb.product_id),
                    mdb.info_frames[i].fisb.length);

            switch(mdb.info_frames[i].fisb.product_id) {
                // Handle METARs, WINDs, PIREPs, TAFs.
                case(FIS_B_ADPU_TEXT_FORMAT_2):
                    handle_uat_text_product(timestamp,
                                            &(mdb.info_frames[i].fisb.data[0]),
                                            mdb.info_frames[i].fisb.length);
                    break;

                // Handle NOTAMs and TFRs.
                case(FIS_B_ADPU_NOTAM):
                    break;

                default:
                    break;
            }
        }
    } else {
        fprintf(stdout, "ERROR: Received unknown message type %d!\n", type);
    }
}

void handle_uat_traffic(struct uat_adsb_mdb* mdb) {
    bool success = false;

    pthread_mutex_lock(&uat_traffic_mutex);
    for(size_t i = 0; i < MAX_TRACKED_TRAFFIC; i++) {
        // We've encountered an empty slot, pop it in!
        if(tracked_traffic[i].address == 0) {
            fprintf(stdout, "Adding new entry for address %04x\n", mdb->address);
            memcpy(&tracked_traffic[i], mdb, sizeof(tracked_traffic[i]));
            success = true;
            break;

        } else if(tracked_traffic[i].address == mdb->address) {
            fprintf(stdout, "Updating entry for address %04x\n", mdb->address);
            memcpy(&tracked_traffic[i], mdb, sizeof(tracked_traffic[i]));
            success = true;
            break;
        }
    }
    pthread_mutex_unlock(&uat_traffic_mutex);

    if(success == false) {
        fprintf(stdout, "ERROR: Ran out of room for traffic.  Max = %d.\n", MAX_TRACKED_TRAFFIC);
    }
}

// Modified copy of dump978's uat_display_fisb_frame.
void handle_uat_text_product(uint64_t timestamp, uint8_t* data, uint16_t length) {
    const char *report = decode_dlac(data, length);
    
    char report_buf[1024];
    const char *next_report;
    char *p, *r;

    struct tm tm;
    time_t rawtime;
    struct tm* current_tm;

    char* product_type = NULL;
    char* location = NULL;
    char* productTime = NULL;

    char message[1024];

    char receivedTime[32];
    char formattedTime[32];

    // Derivative of 'uat_decode.c' -> 'uat_display_fisb_frame'.
    memset(&report_buf[0], 0, sizeof(report_buf));
    while (!report_buf[0]) {
        next_report = strchr(report, '\x1e'); // RS
        if (!next_report) {
            next_report = strchr(report, '\x03'); // ETX
        }

        if (next_report) {
            memcpy(report_buf, report, next_report - report);
            report_buf[next_report - report] = 0;
            report = next_report + 1;
        } else {
            strcpy(report_buf, report);
            break;
        }
    }

    // Init with the beginning of the buffer.    
    r = &(report_buf[0]);

    // Product Type.
    p = strchr(&(report_buf[0]), ' ');
    if(p != NULL) {
        *p = 0;
        product_type = r;
        r = p + 1;
    }

    // Now that we skipped the type, we will make a copy the rest of the message in it's entirely.
    strcpy(&message[0], r);
    
    // Product Location.
    p = strchr(r, ' ');
    if(p != NULL) {
        *p = 0;
        location = r;
        r = p + 1;
    }
    
    // Product Time.
    p = strchr(r, ' ');
    if(p != NULL) {
        *p = 0;
        productTime = r;
        r = p + 1;
    }

    // Assumption is that time we received contains HHMMSSZ.
    strptime(productTime, "%d%H%M%Z", &tm);

    // Snag the current time, for both assembling the current product time and recording when we
    // received this product.
    time(&rawtime);
    current_tm = gmtime(&rawtime);
    strftime(&receivedTime[0], sizeof(receivedTime), "%F %T", current_tm);

    // Copy over the received hour and minute from the product valid time.
    current_tm->tm_hour = tm.tm_hour;
    current_tm->tm_min = tm.tm_min;
    current_tm->tm_sec = 0;
    current_tm->tm_isdst = -1;

    // Check if the day rolled over.
    if(current_tm->tm_mday != tm.tm_mday) {
        current_tm->tm_mday++;
    }
    
    // Fix any overflows from the fudging above.  Dump it in a format that SQLite can work with.
    mktime(current_tm);
    strftime(&formattedTime[0], sizeof(formattedTime), "%F %T", current_tm);

    // Hacky, but strip any trailing newline.
    uint16_t len = strlen(message);
    if(message[len - 1] == 0x0A) {
        message[len - 1] = 0x00;
    }

    database_insert_uat_text_product(&receivedTime[0], product_type, &formattedTime[0],
                                     location, message);
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
    //Modes.oversample              = 1;
    Modes.mode_ac                 = 0;
    Modes.nfix_crc                = MODES_MAX_BITERRORS;
    //Modes.phase_enhance           = 1;
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
                                              &Modes.converter_state);
}

void cleanup_dump1090() {
    //TODO(rdavid): Actually clean up after modes
}
