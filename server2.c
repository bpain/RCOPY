/* Server side - UDP Code				    */
/* By Hugh Smith	4/1/2017	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "PDU.h"
#include "cpe464.h"
#include "packets.h"
#include "pollLib.h"
#include "packets.h"
#include "functions.h"
#include "FIFO.h"


#define IDLE 0 
#define WAIT_DATA 1 
#define IN_ORDER 2
#define BUFFER_DATA 3
#define FLUSH 4 
#define END 5

#define MINSIZE 100
uint8_t init_buffer[MINSIZE]; 

float error_rate = 0.0; 
int window_size = 0;
int buffer_size = 0;
uint32_t sequence_num = 0;
uint32_t send_sequence_num = 0; 
uint8_t* buffer; 

int state = IDLE;
int dataLen = 0; 
int last_sequence_num = -1; 
int last_length; 

struct timeval timeout;
int poll_timeout; 
fd_set readSet;

FILE* fp; 

struct sockaddr_in6 client;		
socklen_t clientAddrLen = sizeof(client);	
char* filename;

int socketNum; 


void processClient();
int checkArgs(int argc, char *argv[]);

pid_t pid = 1; 
int got_sigint = 0; 

void handle_sigint(int sig) {
	got_sigint = 1; 
}

void check_sigint(){
	if(got_sigint == 1){	
		close_FIFO(); 
		if(buffer){
			free(buffer); 
			buffer = NULL; 
		}
		if(fp){
			fclose(fp); 
			fp = NULL; 
		}
		if(filename){
			free(filename); 
			filename = NULL; 
		}
		exit(0); 
	}
}

int main (int argc, char *argv[])
{ 
	error_rate = atof(argv[1]);
	sendErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	signal(SIGINT, handle_sigint);
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
	socketNum = udpServerSetup(portNumber);
	addToPollSet(socketNum); 
	processClient(socketNum);

	close(socketNum);
	
	return 0;
}

FILE* open_file(char* filename){
	FILE *fp = fopen(filename, "wb");
	// printf("filename: %s\n", filename);
    if (fp == NULL) {
        printf("Error on open of output file: %s", filename);
        return 0;
    }
	return fp; 
}

void get_filename_from_PDU(uint8_t* buffer, int dataLen){
	filename = malloc(dataLen - 16 + 1);
	buffer_size = ((struct setup_packet*)init_buffer)->buffer_size;
	window_size = ((struct setup_packet*)init_buffer)->window_size;
	memcpy(filename, (uint8_t*)(&(((struct setup_packet*)init_buffer)->filename_len)), dataLen - 16);//((struct setup_packet*)init_buffer)->filename
	filename[dataLen - 16] = '\0';
	return; 
}


int FUNC_IDLE(){
	check_sigint(); 
	poll_timeout = (pid == 0)? 10000: 0; 
	// memset(buffer, 0, buffer_size + 7); 
	// if(selectMod(socketNum + 1, &readSet, NULL, NULL, &timeout)){
	if(pollCall(poll_timeout) != -1){
		// printf("got packet"); 
		dataLen = recvfromErr(socketNum, init_buffer, MINSIZE, 0, (struct sockaddr *) &client, &clientAddrLen);//safeRecvfrom(socketNum, buffer, MAXBUF + 7, 0, (struct sockaddr *) &client, &clientAddrLen);
		if(pid != 0){
			pid = fork(); 
			if(pid != 0){
				return IDLE; 
			}
			sendErr_init(error_rate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
			// printf("Socket %d\n", socketNum); 
			removeFromPollSet(socketNum); 
			socketNum = createUdpSocket(); 
			addToPollSet(socketNum); 
			// printf("Socket %d\n", socketNum); 
		}
		if(in_cksum((unsigned short*)init_buffer, dataLen) != 0){
			// printf("got corrupted flag packet\n");
			uint32_t temp = 0; 
			send_SREJ(&send_sequence_num, &temp, socketNum, &client, clientAddrLen);
			send_sequence_num++; 
			return IDLE; 
		}
		else if( ((struct generic_packet*)init_buffer)->flag != FILENAME_PACKET){
			// printf("got  wrong flag packet\n");
			uint32_t temp = 0; 
			send_SREJ(&send_sequence_num, &temp, socketNum, &client, clientAddrLen);
			send_sequence_num++; 
			return IDLE; 
		}
		get_filename_from_PDU(init_buffer, dataLen);
		printPDU(init_buffer, dataLen); 
		fp = open_file(filename); 
		if(!fp){
			make_PDU(init_buffer, send_sequence_num, FILE_DNE_PACKET, 0, 0);
			sendtoErr(socketNum, init_buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);
			send_sequence_num++;
			return IDLE; 
		}
		else{
			// printf("got filename sending response"); 
			buffer_size = ntohl(((struct setup_packet*)init_buffer)->buffer_size); 
			window_size = ntohl(((struct setup_packet*)init_buffer)->window_size); 
			buffer = malloc(buffer_size + 7);
			init_FIFO(window_size, buffer_size); 
			memset(buffer, 0, sizeof(struct setup_packet)); 
			send_sequence_num = 0xFFFFFFFF; 
			make_PDU(buffer, send_sequence_num, SETUP_RESPONSE_PACKET, 0, 0);
			sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);
			printPDU(buffer, 7);
			send_sequence_num++; 
			return WAIT_DATA; 
			}
		}
		return (pid == 0) ? END:IDLE; 
		
		

}

int FUNC_WAIT_DATA(){
	check_sigint(); 
	poll_timeout = 10000;//1000000;
	memset(buffer, 0, buffer_size + 7); 
	if(pollCall(poll_timeout) != -1){
		int bytes = recvfromErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) &client, &clientAddrLen);//safeRecvfrom(socketNum, buffer, MAXBUF + 7, 0, (struct sockaddr *) server, &serverAddrLen);
		printPDU(buffer, bytes);
		if(in_cksum((unsigned short*)buffer, bytes) != 0){
			// printf("incorrect initial checksum data packet");
			uint32_t temp = 1; 
			send_SREJ(&send_sequence_num, &temp, socketNum, &client, clientAddrLen);
			send_sequence_num++; 
			return WAIT_DATA;
		}
		else if(((struct generic_packet*)buffer)->flag == DATA_PACKET){//if recieved response go to next state 
			// printf("got data packet\n"); 
			uint32_t packet_num = ntohl(((struct generic_packet*)buffer)->seq_num); 
			dataLen = bytes; 
			if(bytes < dataLen){
				last_length = bytes; 
				last_sequence_num = packet_num; 
			}
			if(packet_num == sequence_num){
				// printf("in order\n"); 
				fwrite(buffer + 7, sizeof(char), bytes - 7, fp);
				// printf("message received:");
				fwrite(buffer + 7, sizeof(char), bytes - 7, stdout);
				//printf("\n"); 
				sequence_num++; 
				send_RR(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen); 
				// make_PDU(buffer, ((struct generic_packet*)buffer)->seq_num, RR_PACKET, 0, 0); 
				// sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);
				send_sequence_num++; 
				return IN_ORDER; 
			}
			else if(packet_num < sequence_num){
				// printf("early packet\n"); 
				send_RR(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen); 
				// make_PDU(buffer, sequence_num, RR_PACKET, 0, 0); 
				// sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);
				send_sequence_num++; 
				return WAIT_DATA; 
			}
			else{
				// printf("out of order packet expected %d, real %d\n", sequence_num, packet_num); 
				send_SREJ(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen);
				write_data(((struct generic_packet*)buffer)); 
				// uint8_t check[107]; 
				// peak(check); 
				// printf("pushing to buffer %d", ntohl(((struct generic_packet*)buffer)->seq_num));
				return BUFFER_DATA;
			}
		}
		else if(((struct generic_packet*)buffer)->flag == SREJ_PACKET){ //if rejected resend packet
			make_PDU(buffer, 0, SETUP_RESPONSE_PACKET, 0, 0);
			sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);			
			send_sequence_num++;	
			return WAIT_DATA; 	
		}
		else if(((struct generic_packet*)buffer)->flag == FILENAME_PACKET){
			dataLen = bytes; 
			make_PDU(buffer, 0, SETUP_RESPONSE_PACKET, 0, 0);
			sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);			
			send_sequence_num++;	
			return WAIT_DATA; 
		}
		else if(((struct generic_packet*)buffer)->flag == LAST_PACKET){
			// printf("file was empty"); 
			return END; 
		}
		else{ //got weird shit 
			//printf("got something other than setup response/data packet\n");
			return END; 
		}
	}
	else{
		printf("data/filename timeout"); 
		return END;		
	}
	return END; 
}

int FUNC_IN_ORDER(){
	check_sigint(); 
	memset(buffer, 0, buffer_size + 7); 
	poll_timeout = 10000; 
	if(pollCall(poll_timeout)!= -1){
		int bytes = recvfromErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) &client, &clientAddrLen);//safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
		if(in_cksum((unsigned short*)buffer, bytes) != 0){
			// printf("incorrect checksum sending SREJ\n"); 
			fflush(stdout); 
			send_SREJ(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen);
			send_sequence_num++; 
			return IN_ORDER;
		}
		else if(((struct generic_packet*)buffer)->flag == DATA_PACKET){
			uint32_t packet_num = ntohl(((struct generic_packet*)buffer)->seq_num); 
			if(bytes < dataLen){
				last_length = bytes; 
				last_sequence_num = packet_num; 
			}
			if(packet_num == sequence_num){
				// printf("correct data packet\n"); 
				fflush(stdout); 
				fwrite(buffer + 7, sizeof(char), bytes - 7, fp);
				// printf("message received:");
				// fwrite(buffer + 7, sizeof(char), dataLen - 7, stdout);
				// printf("\n"); 
				// make_PDU(buffer, packet_num, RR_PACKET, 0, 0); 
				// sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);
				sequence_num ++; 
				send_sequence_num++; 
				send_RR(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen); 	
				return IN_ORDER; 
			}
			else if(packet_num < sequence_num){
				// printf("early sequence number\n"); 
				fflush(stdout); 
				// make_PDU(buffer, sequence_num, RR_PACKET, 0, 0); 
				// sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);
				send_sequence_num++; 
				send_RR(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen); 
				return IN_ORDER;
			}
			else{
				// printf("out of order data expected %d, real %d\n", sequence_num, packet_num); 
				fflush(stdout); 
				send_SREJ(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen);
				//buffer data
				
				write_data(((struct generic_packet*)buffer));  
				send_sequence_num++; 
				return BUFFER_DATA;
			}
		}
		else if(((struct generic_packet*)buffer)->flag == LAST_PACKET){
			// printf("done"); 
			return END; 
		}
		// printf("dataLen: %d\n", dataLen);
		printPDU(buffer, dataLen);
		// fwrite(buffer + 7, sizeof(char), dataLen - 7, fp);
	}
	return END; 
}


int FUNC_BUFFER_DATA(){
	check_sigint(); 
	memset(buffer, 0, buffer_size + 7); 
	poll_timeout = 10000;//10000; 
	if(pollCall(poll_timeout) != -1){
		int bytes = recvfromErr(socketNum, buffer, buffer_size + 7, 0, (struct sockaddr *) &client, &clientAddrLen);//safeRecvfrom(socketNum, buffer, MAXBUF, 0, (struct sockaddr *) &client, &clientAddrLen);
		uint32_t packet_num = ntohl(((struct generic_packet*)buffer)->seq_num); 
		if(in_cksum((unsigned short*)buffer, bytes) != 0){
			// printf("corrupted packet received in buffer\n");
			fflush(stdout);  
			send_SREJ(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen);
			send_sequence_num++; 
			return BUFFER_DATA;
		}
		else if(packet_num == sequence_num){
			// printf("buffer: in order packet received: %d\n", sequence_num);
			fflush(stdout);  
			if(bytes < dataLen){
				last_length = bytes; 
				last_sequence_num = packet_num; 
			}
			if(packet_num == last_sequence_num){
				fwrite(buffer + 7, sizeof(char), last_length - 7, fp);
			}
			else{
				fwrite(buffer + 7, sizeof(char), bytes - 7, fp);
			}
			sequence_num++; 
			return FLUSH; 
		}
		else if(packet_num > sequence_num){
			// printf("buffer: out of order data expected %d, real %d\n", sequence_num, packet_num); 
			fflush(stdout);  
			write_data((struct generic_packet*)buffer); 
			send_RR(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen);
			if(bytes < dataLen){
				last_length = bytes; 
				last_sequence_num = packet_num; 
			}
			return BUFFER_DATA; 
		}
		else if(packet_num < sequence_num){
			// printf("duplicate packet received: %d\n", packet_num); 
			fflush(stdout);  
			send_RR(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen); 
			// make_PDU(buffer, sequence_num, RR_PACKET, 0, 0); 
			// sendtoErr(socketNum, buffer, 7, 0, (struct sockaddr *) &client, clientAddrLen);
			return BUFFER_DATA; 
		}
	}
	return END; 
}

int FUNC_FLUSH(){
	check_sigint(); 
	// printf("flushing\n"); 
	memset(buffer, 0, buffer_size + 7); 
	if(peak(buffer)){
		if(ntohl(((struct generic_packet*)buffer)->seq_num) == sequence_num){
			if(((struct generic_packet*)buffer)->flag == LAST_PACKET){
				return END; 
			}

			// printf("flushing buffer: %d\n", sequence_num); 
			if(read_data()){
				int len = (ntohl(((struct generic_packet*)buffer)->seq_num) == last_sequence_num)? last_length:dataLen; 
				fwrite(buffer + 7, sizeof(char), len - 7, fp);
				sequence_num ++; 
			}
			return FLUSH; 
			
		}
		else{
			// printf("empty/out of order buffer, expected: %d, real: %d\n", sequence_num, ntohl(((struct generic_packet*)buffer)->seq_num));
			fflush(stdout); 
			send_SREJ(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen); 
		 	send_sequence_num++; 
			return BUFFER_DATA; 
		}
	}
	send_RR(&send_sequence_num, &sequence_num, socketNum, &client, clientAddrLen); 
	send_sequence_num++;
	return IN_ORDER; 
}


void processClient()
{
	int state = IDLE;
	// timeout.tv_sec = 10;
	// timeout.tv_usec = 0;
	// FD_SET(socketNum, &readSet);  

	while(1){
		check_sigint(); 
		switch(state){
		case IDLE: 
			state = FUNC_IDLE();
		break;
		case WAIT_DATA:
			state = FUNC_WAIT_DATA();
		break; 
		case IN_ORDER:
			state = FUNC_IN_ORDER();
		break;
		case BUFFER_DATA:
			state = FUNC_BUFFER_DATA(); 
		break; 
		case FLUSH: 
			state = FUNC_FLUSH(); 
		break; 
		case END: 
			if(fp){
				fclose(fp);
				fp = NULL; 
			}
			close_FIFO(); 
			if(buffer){
				free(buffer); 
				buffer = NULL; 
			}
			if(fp){
				fclose(fp); 
				fp = NULL; 
			}
			if(filename){
				free(filename); 
				filename = NULL; 
			}
			exit(0); 
		break; 

	};
	}
	 
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;
	
	if (argc > 3)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	else if(argc < 2){
		fprintf(stderr, "missing error rate"); 
		exit(-1); 
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}


