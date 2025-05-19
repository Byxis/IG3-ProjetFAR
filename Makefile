CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

# Source files and targets
SRC_DIR = .
CLIENT_SRC = $(SRC_DIR)/client.c
SERVER_SRC = $(SRC_DIR)/server.c
CHAINED_LIST_SRC = $(SRC_DIR)/ChainedList.c
CHANNEL_SRC = $(SRC_DIR)/Channel.c

CLIENT = client
SERVER = server
CHAINED_LIST_OBJ = ChainedList.o
CHANNEL_OBJ = Channel.o

all: $(CLIENT) $(SERVER)

# Compilation du serveur
$(SERVER): $(COMMON_OBJS) $(SERVER_OBJS) Channel.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compilation du client
$(CLIENT): $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Règle de compilation des fichiers objets
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(CHANNEL_OBJ): $(CHANNEL_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(CLIENT): $(CLIENT_SRC) $(CHAINED_LIST_OBJ) $(CHANNEL_OBJ)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(CHAINED_LIST_OBJ) $(CHANNEL_OBJ) $(LDFLAGS)

$(SERVER): $(SERVER_SRC) $(CHAINED_LIST_OBJ) $(CHANNEL_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(CHAINED_LIST_OBJ) $(CHANNEL_OBJ) $(LDFLAGS)

clean:
	rm -f $(CLIENT) $(SERVER) *.o

# Dépendances
server.o: server.c ChainedList.h command.h user.h cJSON.h
client.o: client.c
user.o: user.c ChainedList.h command.h user.h cJSON.h
command.o: command.c command.h ChainedList.h user.h
ChainedList.o: ChainedList.c ChainedList.h
cJSON.o: cJSON.c cJSON.h
Channel.o: Channel.c Channel.h

server.o: $(SERVER_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean
