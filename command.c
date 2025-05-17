#include "command.h"
#include "ChainedList.h"
#include <string.h>
#include <stdlib.h>
#include "user.h"
#include <stdio.h>

void sendFileContent(int client, const char *filename);
void upload(int socketFd, const char *filename);
void download(int socketFd, const char *input);
void sendAllClients(const char *message);

/**
 ** Parses a command string and returns the corresponding Command enum.
 * @param msg (const char*) - The command string to parse.
 * @returns Command - The parsed command type.
 */
Command parseCommand(const char *msg)
{
    if (strncasecmp(msg, "@command", 8) == 0)
        return COMMAND;
    if (strncasecmp(msg, "@help", 5) == 0)
        return HELP;
    if (strncasecmp(msg, "@ping", 5) == 0)
        return PING;
    if (strncasecmp(msg, "@msg", 4) == 0)
        return MSG;
    if (strncasecmp(msg, "@connect", 8) == 0)
        return CONNECT;
    if (strncasecmp(msg, "@credits", 8) == 0)
        return CREDITS;
    if (strncasecmp(msg, "@shutdown", 9) == 0)
        return SHUTDOWN;
    if (strncasecmp(msg, "@create", 7) == 0)
        return CREATE;
    if (strncasecmp(msg, "@join", 5) == 0)
        return JOIN;
    if (strncasecmp(msg, "@leave", 6) == 0)
        return LEAVE;
    if (strncasecmp(msg, "@upload", 7) == 0)
        return UPLOAD;
    if (strncasecmp(msg, "@download", 9) == 0)
        return DOWNLOAD;
    return UNKNOWN;
}

/**
 ** Sends a private message to a user if authenticated.
 * @param senderSock (int) - The sender's socket file descriptor.
 * @param username (const char*) - The recipient's username.
 * @param msg (const char*) - The message to send.
 * @returns void
 */
void privateMessage(int senderSock, const char *username, const char *msg)
{
    User *user = findUserByName(username);
    if (user != NULL && user->authenticated)
    {
        char fullMsg[1024];
        snprintf(fullMsg, sizeof(fullMsg), "[privé] %s", msg);
        send(user->socket_fd, fullMsg, strlen(fullMsg), 0);
    }
    else
    {
        char fullMsg[1024];
        snprintf(fullMsg, sizeof(fullMsg), "Utilisateur '%s' introuvable ou non connecté.", username);
        send(senderSock, fullMsg, strlen(fullMsg), 0);
    }
}

/**
 ** Executes a command received from a client socket.
 * Parses the command, dispatches to the appropriate handler, and sends responses in French.
 * @param sock (int) - The client socket file descriptor.
 * @param msg (char*) - The received command string.
 * @returns void
 */
void executeCommand(int sock, char *msg)
{
    char response[1024];
    Command cmd = parseCommand(msg);

    switch (cmd)
    {
    case COMMAND:
        snprintf(response, sizeof(response),
                 "Commandes disponibles:\n"
                 "@help - Affiche l'aide\n"
                 "@ping - Répond 'pong'\n"
                 "@msg <user> <msg> - Message privé\n"
                 "@connect <user> <pwd> - Connexion\n"
                 "@credits - Affiche les crédits\n"
                 "@shutdown - Éteint le serveur\n");
        send(sock, response, strlen(response), 0);
        break;
    case PING:
        send(sock, "pong", 4, 0);
        break;
    case MSG:
    {
        char user[MAX_USERNAME_LENGTH];
        char message[1024];
        sscanf(msg, "@msg %s %[^\n]", user, message);
        privateMessage(sock, user, message);
        break;
    }
    case CONNECT:
    {
        char username[MAX_USERNAME_LENGTH];
        char password[MAX_PASSWORD_LENGTH];
        int parsed = sscanf(msg, "@connect %s %s", username, password);
        if (parsed != 2)
        {
            send(sock, "Commande invalide. Usage : @connect <username> <password>", 61, 0);
            break;
        }
        User *client = findUserByName(username);
        if (client != NULL)
        {
            if (strcmp(client->password, password) == 0)
            {
                client->authenticated = true;
                client->socket_fd = sock;
                send(sock, "Connexion réussie.", 19, 0);
            }
            else
            {
                send(sock, "Mot de passe incorrect.", 24, 0);
            }
        }
        else
        {
            send(sock, "Nom d'utilisateur non trouvé.", 30, 0);
        }
        break;
    }
    case HELP:
        sendFileContent(sock, "README.txt");
        break;
    case CREDITS:
        sendFileContent(sock, "Credits.txt");
        break;
    case SHUTDOWN:
    {
        User *user = findUserBySocket(sock);
        if (user != NULL && getRoleByName(user->name) == ADMIN)
        {
            send(sock, "Arrêt du serveur...", 21, 0);
        }
        else
        {
            send(sock, "Commande réservée à l'admin.", 30, 0);
        }
        break;
    }
    case UPLOAD:
    {
        char filename[100];
        if (sscanf(msg + 8, "%99s", filename) == 1)
        {
            if (strstr(filename, "..") != NULL)
            {
                send(sock, "Nom de fichier invalide.\n", 26, 0);
            }
            else
            {
                upload(sock, filename);
            }
        }
        else
        {
            send(sock, "Nom de fichier manquant.\n", 26, 0);
        }
        break;
    }
    case DOWNLOAD:
        download(sock, msg);
        break;
    default:
    {
        char broadcastMsg[1024];
        snprintf(broadcastMsg, sizeof(broadcastMsg), "Message de %d : %s", sock, msg);
        sendAllClients(broadcastMsg);
        break;
    }
    }
}
