#ifndef WINDOW
#define WINDOW

#include <stdint.h> 

extern void create_window(int size, uint16_t buffer_size); 
extern void move_window(int lower); 
extern int increment_current(uint8_t* data, int length); 
extern void cleanup(); 
extern int check_closed(); 
extern int get_lower(); 
extern uint8_t* get_data(int index); 
extern void print_stuff(); 

#endif