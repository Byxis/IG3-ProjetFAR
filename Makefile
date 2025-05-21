CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

# Files for server
SERVER_OBJS = server.o user.o command.o ChainedList.o Channel.o file.o

# Files for client
CLIENT_OBJS = client.o

all: server client

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f server client *.o

.PHONY: all clean