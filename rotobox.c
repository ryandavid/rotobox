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

#include <rtl-sdr.h>

#include "dump978.h"
#include "dump1090.h"
#include "fec.h"
#include "uat_decode.h"
#include "rotobox.h"

volatile bool exitRequested = false;
rtlsdr_dev_t *device978, *device1090;

void handle_sigint() {
    fprintf(stdout, "Caught SIGINT!\n");
    exitRequested = true;
}

int main(int argc, char **argv) {
    pthread_t thread_978 = NULL, thread_1090 = NULL;

    // Signal Handlers
    signal(SIGINT, handle_sigint);

    // Init from dump1090
    init_dump1090();

    // Init from dump978
    make_atan2_table();
    init_fec();

    device978 = init_SDR("0978", RECEIVER_CENTER_FREQ_HZ_978, RECEIVER_SAMPLING_HZ_978);
    if(device978 != NULL) {
        int success = pthread_create(&thread_978, NULL, dump978_worker, NULL);
        if(success != 0) {
            fprintf(stdout, "Failed to create 978MHz thread (result = %d)\n", success);
        }
    }

    device1090 = init_SDR("1090", RECEIVER_CENTER_FREQ_HZ_1090, RECEIVER_SAMPLING_HZ_1090);
    if(device1090 != NULL) {
        int success = pthread_create(&thread_1090, NULL, dump1090_worker, NULL);
        if(success != 0) {
            fprintf(stdout, "Failed to create 1090MHz thread (result = %d)\n", success);
        }
    }

    //while(exitRequested == false);

    pthread_join(thread_978, NULL);
    pthread_join(thread_1090, NULL);

    cleanup_dump1090();

    rtlsdr_close(device978);
    rtlsdr_close(device1090);
    
    fprintf(stdout, "Exiting!\n");
}


rtlsdr_dev_t *init_SDR(const char *serialNumber, long centerFrequency, int samplingFreq) {
    rtlsdr_dev_t *device = NULL;

    int numDevices = rtlsdr_get_device_count();

    char manufacturer[256], name[256], serial[256];
    for (int i = 0; i < numDevices; i++) {
        if ((rtlsdr_get_device_usb_strings(i, &manufacturer[0], &name[0], &serial[0]) == 0) &
            (strcmp(&serial[0], serialNumber) == 0)) {
            fprintf(stdout, "Opening device %d: %s %s, S/N: %s\n", i, manufacturer, name, serial);

            if (rtlsdr_open(&device, i) == 0) {
                fprintf(stdout, "Successfully opened device!\n");

                rtlsdr_set_tuner_gain_mode(device, 1);  // 1 indicates manual mode
                rtlsdr_set_tuner_gain(device, RECEIVER_GAIN_TENTHS_DB);  // Tenths of a dB
                rtlsdr_set_center_freq(device, centerFrequency);
                rtlsdr_set_sample_rate(device, samplingFreq);
                rtlsdr_reset_buffer(device);
            } else {
                fprintf(stdout, "ERROR: Could not open device!\n");
            }

            // Since we found a match with serial numbers, break out regardless if successful
            break;
        }
    }

    if (device == NULL) {
        fprintf(stdout, "Could not open device with S/N '%s'\n", serialNumber);
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

void dump978_callback(uint64_t timestamp, uint8_t *buffer, int receiveErrors, int type) {
    fprintf(stdout, "\nts=%llu, rs=%d\n", timestamp, receiveErrors);
    if(type == 0) {  // ADS-B
        struct uat_adsb_mdb mdb;
        uat_decode_adsb_mdb(buffer, &mdb);
        uat_display_adsb_mdb(&mdb, stdout);
    } else if(type == 1) {  // UAT
        struct uat_uplink_mdb mdb;
        uat_decode_uplink_mdb(buffer, &mdb);
        uat_display_uplink_mdb(&mdb, stdout);
    } else {
        fprintf(stdout, "ERROR: Received unknown message type %d!\n", type);
    }
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
