CC = g++ -Wall -Wextra -Wconversion -Wshadow -pedantic -std=c++11 -pthread

.PHONY: clean all

all: client server

client: client.cpp utils.h
	$(CC) client.cpp -o client

server: server.cpp utils.h
	$(CC) server.cpp -o server

clean:
	rm -f ./client ./server
