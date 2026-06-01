#ifndef FIFO_FILE 
#define FIFO_FILE

#include "packets.h"

struct FIFO{
    uint32_t read_pointer; 
    uint32_t write_pointer; 
    int largest; 
    struct generic_packet** buffer; 
};

extern void init_FIFO(uint32_t window_size, uint32_t buffer_size); 
extern int write_data(struct generic_packet* packet); 
extern int read_data(); 
extern int peak(uint8_t* output_buffer); 
extern void close_FIFO(); 

#endif