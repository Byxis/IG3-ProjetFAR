#include "command.h"
#include "ChainedList.h"
#include <string.h>
#include <stdlib.h>
#include "user.h"
#include "Channel.h"
#include <stdio.h>

#define MAX_MESSAGE_SIZE 2000

extern User *findUserByName(const char *username);
extern void sendFileContent(int client, const char *filename);
extern void upload(int socketFd, const char *filename);
extern void download(int socketFd, const char *input);
extern void sendAllClients(const char *message);

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
 ** Finds a user by their socket file descriptor.
 * @param sock (int) - The socket file descriptor.
 * @returns User* - The user object or NULL if not found.
 */
User *findUserBySocket(int sock)
{
    return findUserBySocketId(sock); // Use the helper function from Channel.c
}

/**
 ** Gets the role of a user by their name.
 * @param name (const char*) - The user's name.
 * @returns Role - The role of the user.
 */
Role getRoleByName(const char *name)
{
    User *user = findUserByName(name);
    return user ? user->role : USER;
}

/**
 ** Executes a command received from a user.
 * @param user (User*) - The user who sent the command.
 * @param message (const char*) - The received command string.
 * @param shouldShutdown (int*) - Pointer to the shouldShutdown flag.
 * @returns void
 */
void executeCommand(User *user, const char *message, int *shouldShutdown)
{
    int sock = user->socket_fd;
    char response[1024];
    Command cmd = parseCommand(message);
    char msg[MAX_MESSAGE_SIZE]; // Create a modifiable copy
    strncpy(msg, message, MAX_MESSAGE_SIZE - 1);
    msg[MAX_MESSAGE_SIZE - 1] = '\0';

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
                 "@shutdown - Éteint le serveur\n"
                 "@create <channel_name> [max_size] - Crée un canal\n"
                 "@join <channel_name> - Rejoindre un canal\n"
                 "@leave - Quitter le canal\n");
        send(sock, response, strlen(response), 0);
        break;
    case PING:
        send(sock, "pong", 4, 0);
        break;
    case MSG:
    {
        char username[MAX_USERNAME_LENGTH];
        char message[1024];
        sscanf(msg, "@msg %s %[^\n]", username, message);
        privateMessage(sock, username, message);
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
        if (user->role == ADMIN)
        {
            send(sock, "Arrêt du serveur...", 21, 0);
            if (shouldShutdown != NULL)
            {
                *shouldShutdown = 1;
                printf("Shutdown initiated by admin user: %s\n", user->name);
            }
            else
            {
                printf("Warning: shouldShutdown pointer is NULL\n");
            }
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
    case CREATE:
    {
        char channelName[100];
        int maxSize = -1; // Default to unlimited
        if (sscanf(msg + 8, "%99s %d", channelName, &maxSize) >= 1)
        {
            char response[256];
            userCreateAndJoinChannel(channelName, maxSize, user, response, sizeof(response));
            send(sock, response, strlen(response), 0);
        }
        else
        {
            send(sock, "Usage: @create <channel_name> [max_size]", 40, 0);
        }
        break;
    }
    case JOIN:
    {
        char channelName[100];
        if (sscanf(msg + 6, "%99s", channelName) == 1)
        {
            char response[256];
            userJoinChannel(channelName, user, response, sizeof(response));
            send(sock, response, strlen(response), 0);
        }
        else
        {
            send(sock, "Usage: @join <channel_name>", 28, 0);
        }
        break;
    }
    case LEAVE:
    {
        char response[256];
        userLeaveChannel(user, response, sizeof(response));
        send(sock, response, strlen(response), 0);
        break;
    }
    default:
    {
        char broadcastMsg[MAX_MESSAGE_SIZE * 2];
        char *channelName = getUserChannelName(user);
        if (channelName == NULL)
        {
            channelName = "Hub"; // Default channel name
        }
        printf("Channel Name: %s\n", channelName);
        printf("%s-%s: %s\n", channelName, user->name, msg);
        snprintf(broadcastMsg, sizeof(broadcastMsg), "%s-%s: %s", channelName, user->name, msg);
        sendUserChannelMessage(user, broadcastMsg);
        break;
    }
    }
}
