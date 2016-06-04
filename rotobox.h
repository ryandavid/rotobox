#ifndef ROTOBOX_H_
#define ROTOBOX_H_

#define RECEIVER_GAIN_TENTHS_DB     (48 * 10)
#define RECEIVER_CENTER_FREQ_HZ_978 (978UL * 1000000UL)
#define RECEIVER_SAMPLING_HZ_978    (2083334)

#define LIBRTLSDR_MIN_READ_SIZE     512  // At minimum, the read size in bytes

rtlsdr_dev_t *init_SDR(const char *serialNumber, long centerFrequency, int samplingFreq);
void *dump978_worker();
void dump978_callback(uint64_t, uint8_t *, int, int);

#endif  // ROTOBOX_H_
