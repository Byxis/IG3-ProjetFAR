CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

# Object files
SERVER_OBJS = server.o ChainedList.o user.o command.o file.o cJSON.o
CLIENT_OBJS = client.o

# Targets
all: server client

# Server build
server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Client build (only needs client.o)
client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Object files compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o server client

.PHONY: all clean