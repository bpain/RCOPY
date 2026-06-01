#ifndef PACKETS 
#define PACKETS

#include <stdint.h>

//flags
#define SETUP_PACKET 1
#define SETUP_RESPONSE_PACKET 2
#define DATA_PACKET 3   
#define RR_PACKET 5
#define SREJ_PACKET 6
#define FILENAME_PACKET 7
#define FILENAME_RESPONSE_PACKET 8
#define LAST_PACKET 9
#define FILE_DNE_PACKET 10

#define MAXBUF 80


struct  __attribute__((packed)) generic_packet
{
    uint32_t seq_num;
    unsigned short checksum; 
    uint8_t flag; 
    uint8_t* payload; 
};

struct setup_packet
{
    uint32_t seq_num;
    unsigned short checksum; 
    uint8_t flag;
    uint32_t window_size;
    uint32_t buffer_size;
    uint8_t filename_len; 
};

struct  __attribute__((packed)) RJ_RR_PACKET
{
    uint32_t packet_num;
    unsigned short checksum; 
    uint8_t flag; 
    uint32_t sequence_num; 
};

// struct response_packet
// {
//     uint32_t seq_num;
//     unsigned short checksum; 
//     uint8_t flag = SETUP_RESPONSE_PACKET;
//     uint32_t ack_num; 
// };

// struct data_packet
// {
//     uint32_t seq_num;
//     unsigned short checksum; 
//     uint8_t flag = DATA_PACKET;
//     uint8_t* data;
// };



// struct file_response_packet
// {
//     uint32_t seq_num;
//     unsigned short checksum; 
//     uint8_t flag = FILENAME_RESPONSE_PACKET;
//     uint8_t response;
// };
#endif 