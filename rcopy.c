// Client side - UDP Code				    
// By Hugh Smith	4/1/2017		

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
#include <signal.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "PDU.h"
#include "cpe464.h"
#include "packets.h"
#include "functions.h"
#include "window.h"
#include "pollLib.h"

#define MAXBUF 80 //used for startup length 

//RCOPY STATES
#define START 0 
#define WAIT_ACK 1
#define WRITE_DATA 2 
#define READ_RR 3 
#define WAIT_RR 4
#define HANDLE_LAST 5 
#define END 6


uint32_t window_size = 0;
uint32_t buffer_size = 0;

uint8_t* buffer; 

FILE* fp; 
struct timeval timeout; 
int poll_timeout; 
fd_set readSet;

int sequence_num = 0;
int last_sequence_num =0; 
int last_size = 0; 

void talkToServer(int socketNum, struct sockaddr_in6 * server, char** argv);
int readFromStdin(char * buffer);
int checkArgs(int argc, char * argv[]);

void handle_sigint(int sig) {
	cleanup(); 
	if(buffer){
		free(buffer); 
	}
	if(fp){
		fclose(fp);
		fp = NULL;  
	}	
}


int main (int argc, char *argv[])
 {
	signal(SIGINT, handle_sigint);
	int socketNum = 0;				
	struct sockaddr_in6 server;		// Supports 4 and 6 but requires IPv6 struct
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
	sendErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);

	socketNum = setupUdpClientToServer(&server, argv[6], portNumber);
	addToPollSet(socketNum); 
	talkToServer(socketNum, &server, argv);
	
	close(socketNum);
	return 0;
}

void send_filename(char** argv, int socketNum, struct sockaddr_in6 * server, socklen_t serverAddrLen){
	// uint8_t buffer[MAXBUF + 7];
	uint8_t packets[MAXBUF];
	uint32_t net_window = htonl(window_size); 
	memcpy(packets, &net_window, sizeof(net_window));
	uint32_t net_buffer = htonl(buffer_size); 
	memcpy(packets + sizeof(net_window), &net_buffer, sizeof(net_buffer));
	uint8_t len = strlen(argv[2]); 
	memcpy(packets + sizeof(net_window) + sizeof(net_buffer), &len, sizeof(len)); 
	memcpy(packets + sizeof(net_window) + sizeof(net_buffer) + sizeof(len), argv[2], strlen(argv[2]));
	make_PDU((uint8_t*)buffer, 0, FILENAME_PACKET, packets, strlen(argv[2]) + sizeof(net_buffer) + sizeof(net_window) + sizeof(len));
	sendtoErr(socketNum, buffer, strlen(argv[2]) + sizeof(net_window) + sizeof(net_buffer) + 8, 0, (struct sockaddr *) server, serverAddrLen);
	// printf("sent filename\n"); 
	fflush(stdout); 
}

int FUNC_WAIT_ACK(int socketNum, struct sockaddr_in6 * server, socklen_t* serverAddrLen, char** argv){
	// printf("wait ack\n"); 
	fflush(stdout); 
	int counter = 0; 
	poll_timeout = 1000; 
	memset(buffer, 0, buffer_size + 7); 
	while(1){
		// if(selectMod(socketNum + 1, &readSet, NULL, NULL, &timeout)){
		if(pollCall(poll_timeout) != -1){
			counter = 0; 			
			// printf("client updated server port = %d\n", ntohs(server->sin6_port));
			// printf("client addrlen = %d\n", (int)*serverAddrLen);
			int dataLen = recvfromErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, serverAddrLen);//safeRecvfrom(socketNum, buffer, MAXBUF + 7, 0, (struct sockaddr *) server, &serverAddrLen);
			// printf("client updated server port = %d\n", ntohs(server->sin6_port));
			// printf("client addrlen = %d\n", (int)*serverAddrLen);
			
			printPDU(buffer, dataLen);
			if(in_cksum((unsigned short*)buffer, dataLen) != 0){
				// printf("got corrupted packet\n");
				send_filename(argv, socketNum, server, *serverAddrLen);		
				// printf("checksum failed\n");
				// printPDU(buffer, dataLen); 
				// send_SREJ(sequence_num, socketNum, server, serverAddrLen);
				// sequence_num ++; 
			}
			else if(((struct generic_packet*)buffer)->flag == SETUP_RESPONSE_PACKET){//if recieved response go to next state 
				// printf("got setup response packet\n");
				create_window(window_size, buffer_size + 7);
				return WRITE_DATA; 
			}
			else if(((struct generic_packet*)buffer)->flag == FILE_DNE_PACKET){
				// printf("Error: file %s not found.\n", argv[2]);
				return END; 
			}
			else if(((struct generic_packet*)buffer)->flag == SREJ_PACKET){ //if rejected resend filename packet
				send_filename(argv, socketNum, server, *serverAddrLen);		
				// printf("got SREJ packet, resending filename packet\n");
			}
			else{ //got weird shit 
				// printf("got something other than setup response packet\n");
				return END; 
			}
		}
		else{
			send_filename(argv, socketNum, server, *serverAddrLen);
			counter++; 
		}
		if(counter >= 10){
			// printf("filename timeout\n"); 
			return END; 
		}
	}
	
}

int FUNC_WRITE_DATA(int socketNum, struct sockaddr_in6 * server, socklen_t serverAddrLen){
	// printf("write data\n"); 
	fflush(stdout); 
	memset(buffer, 0, buffer_size + 7); 
	poll_timeout = 0; 
	int bytes; 
	while(!check_closed()){
		if((bytes = fread(buffer, sizeof(char), buffer_size, fp))  >= 0){
			// printf("read bytes"); 
			if(bytes == 0){
				// printf("got to last packet");
				sequence_num --; 
				fflush(stdout); 
				return HANDLE_LAST; 	
			}
			uint8_t temp_buffer[1400]; 
    		memcpy(temp_buffer, buffer, bytes);
			make_PDU(buffer, sequence_num, DATA_PACKET, temp_buffer, bytes);
			last_sequence_num = sequence_num; 
			last_size = bytes + 7; 
			sendtoErr(socketNum, buffer, bytes + 7, 0, (struct sockaddr *) server, serverAddrLen);
			// printf("sent data packet, sequence num %d\n", sequence_num); 
			increment_current(buffer, bytes + 7); 
			sequence_num++;
			
		}
		if(pollCall(poll_timeout) != -1){
			return READ_RR; 
		}
		// printf("stuck on poll?"); 
	}
	return WAIT_RR;
}

int FUNC_READ_RR(int socketNum, struct sockaddr_in6 * server, socklen_t* serverAddrLen, char** argv){
	// printf("read RR\n");
	fflush(stdout); 
	memset(buffer, 0, buffer_size + 7); 
	poll_timeout = 0; 
	while(pollCall(poll_timeout) != -1){
		int bytes = recvfromErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, serverAddrLen);//safeRecvfrom(socketNum, buffer, MAXBUF + 7, 0, (struct sockaddr *) server, &serverAddrLen);
		if(in_cksum((unsigned short*)buffer, bytes) == 0){
			// send_SREJ(0, socketNum, server, serverAddrLen);
			if(((struct generic_packet*)buffer)->flag == RR_PACKET){ 
				// if(ntohl(((struct RJ_RR_PACKET*)buffer)->sequence_num) == get_lower() + window_size){				
				// 	make_PDU(buffer, sequence_num + 1, LAST_PACKET, NULL, 0); 
				// 	sendtoErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, serverAddrLen); 
				// 	return END; 
				// }
				move_window(ntohl(((struct RJ_RR_PACKET*)buffer)->sequence_num));
				// printf("read RR moving to %d\n", ntohl(((struct RJ_RR_PACKET*)buffer)->sequence_num)); 
				print_stuff(); 
			}
			else if(((struct generic_packet*)buffer)->flag == SREJ_PACKET){ //if rejected resend filename packet
				memcpy(buffer, get_data(ntohl(((struct generic_packet*)buffer)->seq_num)), buffer_size + 7);
				// make_PDU(buffer, htonl(((struct generic_packet*)buffer)->seq_num), DATA_PACKET, buffer, bytes - 7);
				sendtoErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, *serverAddrLen);
				// printf("SREJ :( %d\n", htonl(((struct generic_packet*)buffer)->seq_num)); 
			}
		}
	}
	return check_closed() ? WAIT_RR : WRITE_DATA;
}

int FUNC_WAIT_RR(int socketNum, struct sockaddr_in6 * server, socklen_t* serverAddrLen, char** argv){
	// printf("wait RR\n"); 
	fflush(stdout); 
	poll_timeout = 1000;
	int counter =0; 
	memset(buffer, 0, buffer_size + 7); 
	while(1){
		if(pollCall(poll_timeout) != -1){
			return READ_RR; 
		}
		else{
			// printf("timeout waiting RR resending lowest: %d\n", get_lower()); 
			fflush(stdout); 
			memcpy(buffer, get_data(get_lower()), buffer_size +7); 
			// uint8_t temp_buffer[1400]; 
    		// memcpy(temp_buffer, buffer, buffer_size + 7);
			// make_PDU(buffer, ntohl(((struct generic_packet*)buffer)->seq_num), DATA_PACKET, temp_buffer, buffer_size);
			if(get_lower() == last_sequence_num){
				sendtoErr(socketNum, buffer, last_size, 0, (struct sockaddr *) server, *serverAddrLen);
			}
			else{
				sendtoErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, *serverAddrLen);	
			}
			if(counter == 10){
				return END; 
			}
			counter++; 
		}
	}
}

int FUNC_HANDLE_LAST(int socketNum, struct sockaddr_in6* server, socklen_t* serverAddrLen){
	// printf("read RR\n");
	fflush(stdout); 
	memset(buffer, 0, buffer_size + 7); 
	poll_timeout = 1000; 
	int counter = 0; 
	if(get_lower() == last_sequence_num + 1){
		make_PDU(buffer, sequence_num + 1, LAST_PACKET, NULL, 0); 
		sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) server, *serverAddrLen);
		return END; 
	}
	while(1){
		if(pollCall(poll_timeout) != -1){
			int bytes = recvfromErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, serverAddrLen);//safeRecvfrom(socketNum, buffer, MAXBUF + 7, 0, (struct sockaddr *) server, &serverAddrLen);
			if(in_cksum((unsigned short*)buffer, bytes) == 0){
				if(((struct generic_packet*)buffer)->flag == RR_PACKET){ 
					if(ntohl(((struct RJ_RR_PACKET*)buffer)->sequence_num) == last_sequence_num + 1){
						make_PDU(buffer, sequence_num + 1, LAST_PACKET, NULL, 0); 
						sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) server, *serverAddrLen);
						return END; 
					}
					move_window(ntohl(((struct RJ_RR_PACKET*)buffer)->sequence_num));
					// printf("read RR moving to %d\n", ntohl(((struct RJ_RR_PACKET*)buffer)->sequence_num)); 
				}
				else if(((struct generic_packet*)buffer)->flag == SREJ_PACKET){ //if rejected resend filename packet
					if(((struct generic_packet*)buffer)->seq_num == last_sequence_num+1){
						memcpy(buffer, get_data(ntohl(((struct generic_packet*)buffer)->seq_num)), last_size);
						sendtoErr(socketNum, buffer, last_size, 0, (struct sockaddr *) server, *serverAddrLen);
					}
					else{
						memcpy(buffer, get_data(ntohl(((struct generic_packet*)buffer)->seq_num)), buffer_size + 7);
						sendtoErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, *serverAddrLen);	
					}
					// printf("SREJ :( %d\n", htonl(((struct generic_packet*)buffer)->seq_num)); 
				}
			}
		}
		else{
			// printf("LAST timeout waiting RR resending lowest: %d\n", get_lower()); 
			fflush(stdout); 
			memcpy(buffer, get_data(get_lower()), buffer_size +7); 
			// printf("%d bruhhhhhh", ntohl(((struct generic_packet*)get_data(get_lower()))->seq_num));
			if(get_lower() == last_sequence_num){
				sendtoErr(socketNum, buffer, last_size, 0, (struct sockaddr *) server, *serverAddrLen);
			}
			else{
				sendtoErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) server, *serverAddrLen);
			}
			// printf("counter = %d", counter); 
			if(counter == 10){
				// printf("ending from timeout"); 
				return END; 
			}
			counter++; 
		}
	}
}

int FUNC_START(int socketNum, struct sockaddr_in6 * server, socklen_t serverAddrLen, char* argv[]){
	buffer[0] = '\0';
	fp = fopen(argv[1], "rb");
	if (fp == NULL) {
		printf("Error: file %s not found.", argv[1]);
		exit(0); 
	}
	// make_PDU((uint8_t*)buffer, sequence_num, FILENAME_PACKET, (uint8_t*)argv[2], strlen(argv[2]));
	// sendtoErr(socketNum, buffer, strlen(argv[2]) + 7, 0, (struct sockaddr *) server, serverAddrLen);//safeSendto(socketNum, buffer, strlen(argv[1]) + 7, 0, (struct sockaddr *) server, serverAddrLen);
	send_filename(argv, socketNum, server, serverAddrLen);
	memset(buffer, 0, buffer_size + 7);
	sequence_num = 0;
	return WAIT_ACK; 
}


void talkToServer(int socketNum, struct sockaddr_in6 * server, char* argv[])
{
	socklen_t serverAddrLen = sizeof(struct sockaddr_in6);
	buffer = (uint8_t*)malloc(1407);//(buffer_size + 8);
	int state = START;
	while(1){
		switch(state){
			case START: 
				state = FUNC_START(socketNum, server, serverAddrLen, argv);
				break;
			case WAIT_ACK:
				state = FUNC_WAIT_ACK(socketNum, server, &serverAddrLen, argv);
				//wait for ack of filename packet
				break;
			case WRITE_DATA:
				//write data to server
				state = FUNC_WRITE_DATA(socketNum, server, serverAddrLen);
				break; 
			case READ_RR:
				state = FUNC_READ_RR(socketNum, server, &serverAddrLen, argv); 
			break; 
			case WAIT_RR:
				state = FUNC_WAIT_RR(socketNum, server, &serverAddrLen, argv); 
			break; 
			case HANDLE_LAST:
				//handle last packet stuff
				state = FUNC_HANDLE_LAST(socketNum, server, &serverAddrLen); 
				break;
			case END: 
				//close file and exit
				cleanup(); 
				close(socketNum);
				exit(0); 			
				break; 
			default:
				break;
			}
	}
}

int readFromStdin(char * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("Enter data: \n");
	while (inputLen < (buffer_size + 7 - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

int checkArgs(int argc, char * argv[])
{
    int portNumber = 0;
	
    /* check command line arguments  */
	
	if (argc != 8)
	{
		printf("usage: %s from-filename to-filename window-size buffer-size error-rate host-name port-number \n", argv[0]);
		exit(1);
	}
	int num; 
	if((num = strlen(argv[1])) > 100){
		printf("filename 1 too long: strlen == %d, max is 100", num); 
		exit(1); 
	}	
	if((num = strlen(argv[2])) > 100){
		printf("filename 2 too long: strlen == %d, max is 100", num); 
		exit(1); 
	}
	window_size = atoi(argv[3]);
	buffer_size = atoi(argv[4]);

	portNumber = atoi(argv[7]);
		
	return portNumber;
}





