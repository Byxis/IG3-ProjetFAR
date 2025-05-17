CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread

# Fichiers sources communs
COMMON_SRCS = ChainedList.c

# Fichiers sources spécifiques au serveur
SERVER_SRCS = server.c user.c command.c cJSON.c

# Fichiers sources spécifiques au client
CLIENT_SRCS = client.c

# Génération des noms des fichiers objets
COMMON_OBJS = $(COMMON_SRCS:.c=.o)
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# Exécutables
SERVER = server
CLIENT = client

# Règle par défaut
all: $(SERVER) $(CLIENT)

# Compilation du serveur
$(SERVER): $(COMMON_OBJS) $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compilation du client
$(CLIENT): $(COMMON_OBJS) $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Règle de compilation des fichiers objets
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Règle pour nettoyer le projet
clean:
	rm -f *.o $(SERVER) $(CLIENT) *~ core

# Dépendances
server.o: server.c ChainedList.h command.h user.h cJSON.h
client.o: client.c
user.o: user.c ChainedList.h command.h user.h cJSON.h
command.o: command.c command.h ChainedList.h user.h
ChainedList.o: ChainedList.c ChainedList.h
cJSON.o: cJSON.c cJSON.h

.PHONY: all clean