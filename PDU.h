#ifndef PDU
#define PDU
#include <stdint.h>

extern int make_PDU(uint8_t* PDU_BUFFER, uint32_t sequence_number, uint8_t flag, uint8_t* payload, int payload_len); 
extern void printPDU(uint8_t* aPDU, int pduLength); 

#endif 