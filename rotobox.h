#ifndef ROTOBOX_H_
#define ROTOBOX_H_

#define RECEIVER_GAIN_TENTHS_DB     (48 * 10)
#define RECEIVER_CENTER_FREQ_HZ_978 (978UL * 1000000UL)
#define RECEIVER_SAMPLING_HZ_978    (2083334)

rtlsdr_dev_t *init_SDR(const char *serialNumber, long centerFrequency, int samplingFreq);


#endif  // ROTOBOX_H_
