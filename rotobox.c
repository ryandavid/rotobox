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

#include "rotobox.h"
#include "dump978.h"
#include "uat_decode.h"
#include "fec.h"

volatile bool exitRequested = false;
rtlsdr_dev_t *device978;

void handle_sigint() {
    fprintf(stdout, "Caught SIGINT!\n");
    exitRequested = true;
}

int main(int argc, char **argv) {
    pthread_t thread_978;

    // Signal Handlers
    signal(SIGINT, handle_sigint);

    // Init from dump978
    make_atan2_table();
    init_fec();

    device978 = init_SDR("0978", RECEIVER_CENTER_FREQ_HZ_978, RECEIVER_SAMPLING_HZ_978);
    if(device978 != NULL) {
        int success = pthread_create(&thread_978, NULL, dump978_worker, NULL);
        if(success != 0) {
            fprintf(stdout, "Failed to create thread (result = %d)\n", success);
        }
    }

    while(exitRequested == false);

    pthread_join(thread_978, NULL);
    rtlsdr_close(device978);
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

    if(device978 != NULL) {
        while(exitRequested == false) {
            // Hacky way to get the number of bytes in multiples of LIBRTLSDR_MIN_READ_SIZE
            int readLength = ((sizeof(dump978_buffer) - bytesUsed) / LIBRTLSDR_MIN_READ_SIZE) * LIBRTLSDR_MIN_READ_SIZE;
            int readResult = rtlsdr_read_sync(device978, &dump978_buffer[0] + bytesUsed, readLength, &numBytesRead);

            if(readResult != 0){
                // TODO(rdavid): Do something smart to try and recover
                fprintf(stdout, "ERROR: Device read returned %d\n", readResult);
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


