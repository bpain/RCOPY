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

struct buffer{
    uint32_t write_pointer; 
    uint16_t size; 
    uint8_t** data; 
};

struct window{
    int lower_bound; 
    int upper_bound; 
    int current; 
    int size;
}; 

struct window *w;
struct buffer *b;

void create_window(int size, uint16_t buffer_size){
    w = (struct window*)malloc(sizeof(struct window)); 
    b = (struct buffer*)malloc(sizeof(struct buffer));
    b->data = (uint8_t**)calloc(size, sizeof(uint8_t*)); 
    b->size = buffer_size;
    b->write_pointer = 0;
    w->lower_bound = 0; 
    w->upper_bound = size - 1; 
    w->current = 0; 
    w->size = size; 
    return; 
}

void move_window(int lower){
    w->lower_bound = lower; 
    w->upper_bound = lower + w->size - 1;
    return;    
}

int increment_current(uint8_t* data, int length){
    // if(w->current + 1 > w->upper_bound){
    //     return -1; 
    // }
    // if(b->data[(b->write_pointer)%(w->size)] != NULL){
    //     free(b->data[(b->write_pointer)%(b->window_size)]);
    // }
    if(b->data[(b->write_pointer)%(w->size)] == NULL){
        b->data[(b->write_pointer)%(w->size)] = (uint8_t*)calloc(1, b->size + 7); 
    }
    memcpy(b->data[b->write_pointer%(w->size)], data, length); 
    b->write_pointer = (b->write_pointer + 1) % w->size;
    w->current++;
    return 0;
}

void cleanup(){
    for(int i = 0; i < w->size; i++){
        if(b->data[i] != NULL){
            free(b->data[i]);
            b->data[i] = NULL; 
        }
    }
    if(b->data) free(b->data);
    if(b){
        free(b);
        b = NULL; 
    } 
    if(w){
        free(w); 
        w = NULL; 
    } 
    return; 
}

int check_closed(){
    return (w->size == 1) ? (w->current > w->upper_bound):(w->current >= w->upper_bound); 
    //(w->current >= w->upper_bound) || ((w->size == 1) && (w->current > w->upper_bound));
}

int get_lower(){
    return w->lower_bound;
}

uint8_t* get_data(int index){
    return b->data[index % w->size];
}

void print_stuff(){
    printf("lower bound: %d upper bound: %d current %d\n", w->lower_bound, w->upper_bound, w->current); 
}



