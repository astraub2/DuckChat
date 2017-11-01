CC=g++

CFLAGS=-Wall -W -g  



all: client server

client: client.cpp raw.c
	$(CC) client.cpp raw.c $(CFLAGS) -o client

# server: server.cpp raw.c
# 	$(CC) server.cpp $(CFLAGS) -o server

clean:
	rm -f client  *.o
