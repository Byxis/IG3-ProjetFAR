#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>

#define MAX_USERNAME_LENGTH 32
#define MAX_PASSWORD_LENGTH 32
#define MAX_COMMAND_LENGTH 20

// Enumération des types de commandes
typedef enum command{
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
    UNKNOWN,
} Command; 

// Fonction pour parser une commande à partir d'une chaîne
Command parseCommand(const char *msg);

// Fonction pour exécuter une commande
void executeCommand(int sock, char* msg, int shutdown);

#endif
