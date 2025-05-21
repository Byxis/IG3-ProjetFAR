#ifndef COMMAND_H
#define COMMAND_H

#include "user.h"

#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_COMMAND_LENGTH 20

// Enumération des types de commandes
typedef enum command
{
    COMMAND,
    PING,
    MSG,
    HELP,
    CREDITS,
    CONNECT,
    SHUTDOWN,
    CREATE,
    JOIN,
    LEAVE,
    UPLOAD,
    DOWNLOAD,
    DELETE_CHANNEL,
    UNKNOWN,
} Command;

// Fonction pour parser une commande à partir d'une chaîne
Command parseCommand(const char *msg);

// Execute a command based on the message from the user
void executeCommand(User *user, const char *message, int *shouldShutdown);

#endif // COMMAND_H
