#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "PDU.h"
#include "cpe464.h"
#include "packets.h"
#include "functions.h"
#include "FIFO.h"



struct FIFO* read_buffer; 
uint32_t g_window_size;
uint32_t g_buffer_size;
uint32_t g_packet_size;


void init_FIFO(uint32_t window_size, uint32_t buffer_size){
    g_window_size = window_size; 
    g_buffer_size = buffer_size; 
    g_packet_size = 7 + (g_buffer_size -1); 
    read_buffer =(struct FIFO*)malloc(sizeof(struct FIFO)); 
    read_buffer->read_pointer = 0; 
    read_buffer->write_pointer = 0; 
    read_buffer->largest = -1; 
    read_buffer->buffer = (struct generic_packet**)calloc(window_size,sizeof(void*)); 
}

int write_data(struct generic_packet* packet){
    if(((read_buffer->write_pointer + 1)%g_window_size) == (read_buffer->read_pointer)){
        printf("FIFO overflow"); 
        return 0; 
    }
    if(!read_buffer->buffer[read_buffer->write_pointer]){
        read_buffer->buffer[read_buffer->write_pointer] = malloc(g_packet_size+7); 
    }
    if((int)ntohl(packet->seq_num) <= read_buffer->largest){
        printf("number out of order"); 
        return 0; 
    }
    read_buffer->largest = ntohl(packet->seq_num);
    memcpy(read_buffer->buffer[read_buffer->write_pointer], packet, g_packet_size); 
    read_buffer->write_pointer = (((read_buffer->write_pointer)+1) % g_window_size); 
    return 1; 
}

int read_data(){
    if(read_buffer->read_pointer == read_buffer->write_pointer){
        printf("reading empty buffer"); 
        return 0; 
    }
    // memcpy(output_buffer, read_buffer->buffer[read_buffer->read_pointer], g_packet_size); 
    read_buffer->read_pointer = (read_buffer->read_pointer+ 1) % g_window_size; 
    return 1; 
}

int peak(uint8_t* output_buffer){
    if(read_buffer->read_pointer == read_buffer->write_pointer){
        return 0; 
    }
    memcpy(output_buffer, read_buffer->buffer[read_buffer->read_pointer], g_packet_size); 
    return 1; 
}

void close_FIFO(){
    if(read_buffer){
        if(read_buffer -> buffer){
            for(int i = 0; i < g_window_size; i++){
                if(read_buffer->buffer[i]){
                    free(read_buffer->buffer[i]); 
                    read_buffer->buffer[i] = NULL; 
                }
                }
            free(read_buffer->buffer); 
            read_buffer->buffer = NULL; 
        }
        free(read_buffer); 
        read_buffer = NULL; 
    }
}






