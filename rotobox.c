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

bool exitRequested = false;

void handle_sigint() {
    exitRequested = true;
}

int main(int argc, char **argv) {
    // Init from dump978
    make_atan2_table();
    init_fec();

    rtlsdr_dev_t *device978 = init_SDR("0978", RECEIVER_CENTER_FREQ_HZ_978, RECEIVER_SAMPLING_HZ_978);

    rtlsdr_close(device978);
    fprintf(stdout, "Hello World!\n");
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
