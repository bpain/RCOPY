#ifndef functions 
#define functions

#include <netinet/in.h>
#include <netdb.h>

extern void send_SREJ(uint32_t* sequence_num, uint32_t* rej_seq_num, int socketNum,  struct sockaddr_in6 * server, socklen_t serverAddrLen); 
extern void send_RR(uint32_t* sequence_num, uint32_t* RR_seq_num, int socketNum,  struct sockaddr_in6 * server, socklen_t serverAddrLen); 


#endif 