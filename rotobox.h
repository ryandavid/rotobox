#ifndef ROTOBOX_H_
#define ROTOBOX_H_

#include "dump978.h"
#include "fec.h"
#include "uat_decode.h"

#define WEBSERVER_PORT              ("80")

#define RECEIVER_GAIN_TENTHS_DB     (48 * 10)
#define RECEIVER_CENTER_FREQ_HZ_978 (978UL * 1000000UL)
#define RECEIVER_SAMPLING_HZ_978    (2083334)

#define RECEIVER_CENTER_FREQ_HZ_1090 (1090UL * 1000000UL)
#define RECEIVER_SAMPLING_HZ_1090    (2400000)

#define GPSD_ADDRESS_BUFFER_SIZE    (16)
#define GPSD_DEFAULT_PORT           ("2947")

#define LIBRTLSDR_MIN_READ_SIZE     512  // At minimum, the read size in bytes

#define TIME_STRING_LEN         10
#define TYPE_STRING_LEN         10
#define LOCATION_STRING_LEN     10
#define REPORT_STRING_LEN       256


#define MAX_DECODED_UAT_TEXT    (256)
struct uat_decoded_text_frame_t {
    uint64_t rx_timestamp;
    char time[TIME_STRING_LEN];
    char type[TYPE_STRING_LEN];
    char location[LOCATION_STRING_LEN];
    char report[REPORT_STRING_LEN];
};

rtlsdr_dev_t *init_SDR(const char *serialNumber, long centerFrequency, int samplingFreq);

void init_dump978();
void *dump978_worker();
void dump978_callback(uint64_t, uint8_t *, int, frame_type_t);
void cleanup_dump978();

void handle_uat_text_product(uint64_t timestamp, uint8_t * data, uint16_t length);

void init_dump1090();
void *dump1090_worker();
void dump1090_callback(struct modesMessage *mm);
void cleanup_dump1090();



#endif  // ROTOBOX_H_
