#ifndef DUMP978_H_
#define DUMP978_H_

typedef enum {
    FRAME_TYPE_ADSB,
    FRAME_TYPE_UAT
} frame_type_t;

#ifndef MAKE_DUMP_978_LIB
int process_buffer(uint16_t *phi, int len, uint64_t offset);
#else
int process_buffer(uint16_t *phi, int len, uint64_t offset, void (*callback)(uint64_t, uint8_t *, int, frame_type_t));
#endif
void make_atan2_table();
void convert_to_phi(uint16_t *buffer, int n);

#endif  // DUMP978_H_