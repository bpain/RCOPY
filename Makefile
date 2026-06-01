# udpCode makefile
# written by Hugh Smith - Feb 2021

CC = gcc
# -fsanitize=address 
CFLAGS = -fno-omit-frame-pointer -g -Wall -std=gnu99 -fsanitize=address


SRC = networks.c  gethostbyname.c safeUtil.c pollLib.c 
OBJS = networks.o gethostbyname.o safeUtil.o pollLib.o

#uncomment next two lines if your using sendtoErr() library
LIBS += libcpe464.2.21.a -lstdc++ -ldl
CFLAGS += -D__LIBCPE464_

all:  functions udpClient udpServer 

udpClient: rcopy.c $(OBJS) 
	$(CC) $(CFLAGS) -o rcopy rcopy.c PDU.c PDU.h packets.h functions.h functions.c checksum.h checksum.c window.c window.h $(OBJS) $(LIBS)

udpServer: server.c $(OBJS) 
	$(CC) $(CFLAGS) -o server server.c PDU.c PDU.h FIFO.c FIFO.h packets.h functions.h functions.c checksum.h checksum.c $(OBJS) $(LIBS)

functions: functions.c 
	$(CC) $(CFLAGS) -c functions.c
%.o: %.c *.h 
	gcc -c $(CFLAGS) $< -o $@ 

cleano:
	rm -f *.o

clean:
	rm -f rcopy server *.o

