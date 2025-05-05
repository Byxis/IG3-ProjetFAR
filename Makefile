CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = 

# Source files and targets
SRC_DIR = .
CLIENT_SRC = $(SRC_DIR)/client.c
SERVER_SRC = $(SRC_DIR)/server.c

CLIENT = client
SERVER = server

all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDFLAGS)

$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS)

clean:
	rm -f $(CLIENT) $(SERVER) *.o

# For individual compilation if needed
client.o: $(CLIENT_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

server.o: $(SERVER_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean
