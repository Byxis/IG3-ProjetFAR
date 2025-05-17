CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

# Source files and targets
SRC_DIR = .
CLIENT_SRC = $(SRC_DIR)/client.c
SERVER_SRC = $(SRC_DIR)/server.c
CHAINED_LIST_SRC = $(SRC_DIR)/ChainedList.c
FILE_SRC = $(SRC_DIR)/file.c

CLIENT = client
SERVER = server

CHAINED_LIST_OBJ = ChainedList.o
FILE_OBJ = file.o

# Server build
server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Client build (only needs client.o)
client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Object files compilation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(FILE_OBJ): $(FILE_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(CLIENT): $(CLIENT_SRC) $(CHAINED_LIST_OBJ) $(FILE_OBJ)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(CHAINED_LIST_OBJ) $(FILE_OBJ) $(LDFLAGS)

$(SERVER): $(SERVER_SRC) $(CHAINED_LIST_OBJ) $(FILE_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(CHAINED_LIST_OBJ) $(FILE_OBJ) $(LDFLAGS)

clean:
	rm -f *.o server client

.PHONY: all clean