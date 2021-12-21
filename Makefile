CC = clang
CFLAGS = -Wall -Wpedantic -Werror -Wextra -Wshadow -pthread

all: server client

server: server.o list.o
	$(CC) $(CFLAGS) -o server list.o server.o

client: client.o 
	$(CC) $(CFLAGS) -o client client.o 

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

list.o: list.c
	$(CC) $(CFLAGS) -c list.c

clean:
	rm -f server client *.o

