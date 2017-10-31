CC=g++

CFLAGS=-Wall -W -g -Werror 



all: client #server

client: client.cpp raw.c
	$(CC) client.cpp raw.c $(CFLAGS) -o client

server: server 
# 	$(CC) server.c $(CFLAGS) -o server

clean:
	rm -f client server *.o

