#include "packets.h"
#include "functions.h"
#include "safeUtil.h"
#include "PDU.h"
#include "cpe464.h"
#include "networks.h"
#include "gethostbyname.h"


void send_SREJ(uint32_t* sequence_num, uint32_t* rej_seq_num, int socketNum,  struct sockaddr_in6 * server, socklen_t serverAddrLen){
	uint8_t buffer[MAXBUF + 7];
	make_PDU(buffer, *sequence_num, SREJ_PACKET, (uint8_t*)rej_seq_num, 4);
	printf("sending SREJ, seq num: %d\n", *rej_seq_num); 
	sendtoErr(socketNum, buffer, 11, 0, (struct sockaddr *) server, serverAddrLen);
}

void send_RR(uint32_t* sequence_num, uint32_t* RR_seq_num, int socketNum,  struct sockaddr_in6 * server, socklen_t serverAddrLen){
	uint8_t buffer[MAXBUF + 7];
	make_PDU(buffer, *sequence_num, RR_PACKET, ((uint8_t*)RR_seq_num), 4);
	// printf("RR sequence number %d, real sequence number: %d\n", ntohl(((struct RJ_RR_PACKET*)buffer)->sequence_num), *RR_seq_num); 
	sendtoErr(socketNum, buffer, 11, 0, (struct sockaddr *) server, serverAddrLen);
}