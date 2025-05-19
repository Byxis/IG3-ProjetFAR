#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "ChainedList.h"
#include "command.h"
#include "user.h"
#include "Channel.h"
#include "cJSON.h"
#include <sys/stat.h>

#define MAX_MESSAGE_SIZE 2000

int shouldShutdown = 0; // Initialize the variable properly
List *client_sockets;

// Change from definitions to extern declarations
UserList *global_users = NULL;
extern ChannelList channelList;

// Function prototypes
void *handle_client(void *arg);
void sendFileContent(int client, const char *filename);
void upload(int socketFd, const char *filename);
void download(int socketFd, const char *input);
void create_directory(const char *dir);
void handle_login(int client_socket);
User *findUserByName(const char *username);
void add_client(User *user);
void remove_client(int socket_fd);
bool registerUserInHub(User *user);
void disconnectUserFromAllChannels(User *user);
void sendAllClients(const char *message);

User *findUserByName(const char *username)
{
    // Use the thread-safe version
    return findUserByNameSafe(global_users, username);
}

void add_client(User *user)
{
    // Vérifier si l'utilisateur existe avant de l'ajouter
    if (user == NULL)
    {
        fprintf(stderr, "Error: Trying to add NULL user to client list\n");
        return;
    }

    // Vérifier si client_sockets est NULL
    if (client_sockets == NULL)
    {
        client_sockets = createList(user);
        return;
    }

    // List functions now handle their own locking
    addLast(client_sockets, user);
}

void remove_client(int socket_fd)
{
    if (client_sockets == NULL)
        return;

    // Need to lock for scanning through client list
    lockList(client_sockets);

    Node *current = client_sockets->first;
    Node *prev = NULL;

    while (current != NULL)
    {
        if (current->user != NULL && current->user->socket_fd == socket_fd)
        {
            if (prev == NULL)
            {
                client_sockets->first = current->next;
                if (client_sockets->curr == current)
                {
                    client_sockets->curr = current->next;
                }
            }
            else
            {
                prev->next = current->next;
                if (client_sockets->curr == current)
                {
                    client_sockets->curr = prev;
                }
            }

            close(current->user->socket_fd);

            disconnectUserFromAllChannels(current->user);

            free(current->user);
            free(current);
            client_sockets->size--;

            unlockList(client_sockets);
            return;
        }

        prev = current;
        current = current->next;
    }

    unlockList(client_sockets);
}

void *handle_client(void *arg)
{
    int sock = *((int *)arg);
    free(arg);

    User *user = NULL;
    handle_login(sock);

    // Find the user with this socket_fd
    lockList(client_sockets);
    Node *current = client_sockets->first;
    while (current != NULL)
    {
        // Vérifier d'abord si current et current->user ne sont pas NULL
        if (current != NULL && current->user != NULL && current->user->socket_fd == sock)
        {
            user = current->user;
            break;
        }
        current = current->next;
    }
    unlockList(client_sockets);

    if (user == NULL)
    {
        printf("Error: Could not find user for socket %d\n", sock);
        close(sock);
        return NULL;
    }
    printf("Client %s (%d) connecté\n", user->name, sock);

    if (!registerUserInHub(user))
    {
        printf("Error: Could not register user in Hub channel\n");
        close(sock);
        return NULL;
    }
    printf("Client %s (%d) enregistré dans le Hub\n", user->name, sock);

    char buffer[MAX_MESSAGE_SIZE];

    while (1)
    {
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0)
        {
            break;
        }

        buffer[received] = '\0';
        executeCommand(user, buffer, &shouldShutdown);
    }

    printf("Client %s (%d) déconnecté\n", user->name, sock);
    remove_client(sock);
    return NULL;
}

void sendFileContent(int client, const char *filename)
{
    FILE *file = fopen(filename, "r");

    if (!file)
        return;

    char line[1024];

    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';
        send(client, line, strlen(line), 0);
        send(client, "\n", 1, 0);
    }
    fclose(file);
    send(client, "__END__", 7, 0);
}

void create_directory(const char *dir)
{
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        if (mkdir(dir, 0700) != 0)
        {
            perror("Erreur lors de la création du répertoire");
            return;
        }
    }
}

void upload(int socketFd, const char *filename)
{
    if (strstr(filename, "..") != NULL)
    {
        send(socketFd, "Nom de fichier invalide.\n", 26, 0);
        return;
    }

    printf("Démarrage de l'upload du fichier: %s\n", filename);
    create_directory("uploads");
    printf("Répertoire 'uploads' vérifié.\n");

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);
    printf("Sauvegarde vers: %s\n", filepath);
    FILE *fp = fopen(filepath, "wb");

    if (fp == NULL)
    {
        perror("Erreur lors de l'ouverture du fichier pour écriture");
        send(socketFd, "Erreur serveur: impossible de créer le fichier.\n", 48, 0);
        return;
    }
    char buffer[1024];
    int len;
    int total = 0;
    printf("En attente des données...\n");

    while ((len = recv(socketFd, buffer, sizeof(buffer), 0)) > 0)
    {
        if (len == 7 && strncmp(buffer, "__END__", 7) == 0)
        {
            printf("Marqueur de fin détecté.\n");
            break;
        }
        total += len;
        printf("\rRéception: %d/%d octets (%.1f%%)", total, total, 100.0);
        fflush(stdout);
        fwrite(buffer, 1, len, fp);
        fflush(fp);
    }
    fclose(fp);
    printf("\nFichier '%s' (%d octets) reçu et sauvegardé dans 'uploads/'.\n", filename, total);
    send(socketFd, "Fichier reçu avec succès\n", 26, 0);
}

void download(int socketFd, const char *input)
{
    char filename[256] = {0};
    char filepath[512] = {0};
    char response[1024] = {0};
    printf("Commande download reçue: %s\n", input);

    if (sscanf(input + 10, "%255s", filename) != 1)
    {
        strcpy(response, "Erreur: Format incorrect. Utilisation: @download nom_fichier\n");
        send(socketFd, response, strlen(response), 0);
        return;
    }

    snprintf(filepath, sizeof(filepath), "uploads/%s", filename);
    FILE *file = fopen(filepath, "rb");

    if (!file)
    {
        sprintf(response, "Erreur: Le fichier '%s' n'existe pas dans le répertoire uploads.\n", filename);
        send(socketFd, response, strlen(response), 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    fclose(file);
    sprintf(response, "READY_TO_SEND:%s:%ld", filename, file_size);
    printf("Envoi de la réponse: %s\n", response);
    send(socketFd, response, strlen(response), 0);

    char confirm[32] = {0};
    int confirm_recv = recv(socketFd, confirm, sizeof(confirm), 0);

    if (confirm_recv <= 0)
    {
        printf("Erreur: Pas de confirmation du client\n");
        return;
    }
    if (strcmp(confirm, "READY") == 0)
    {
        FILE *f = fopen(filepath, "rb");

        if (!f)
        {
            strcpy(response, "Erreur: Impossible d'ouvrir le fichier pour l'envoi.\n");
            send(socketFd, response, strlen(response), 0);
            return;
        }

        char buffer[1024];
        size_t bytes_read;
        long sent = 0;

        while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0)
        {
            int bytes_sent = send(socketFd, buffer, bytes_read, 0);
            if (bytes_sent <= 0)
            {
                printf("Erreur d'envoi: %d\n", bytes_sent);
                break;
            }
            sent += bytes_sent;
            printf("\rEnvoi: %ld/%ld octets (%.1f%%)", sent, file_size, (float)sent / file_size * 100);
            fflush(stdout);
        }

        fclose(f);
        send(socketFd, "__END__", 7, 0);
        usleep(50000);

        if (sent == file_size)
        {
            sprintf(response, "Fichier '%s' (%ld octets) envoyé avec succès.\n", filename, file_size);
            printf("\nFichier '%s' (%ld octets) envoyé avec succès.\n", filename, file_size);
        }
        else
        {
            sprintf(response, "Erreur: Transfert incomplet. Envoyé %ld/%ld octets.\n", sent, file_size);
            printf("\nErreur: Transfert incomplet. Envoyé %ld/%ld octets.\n", sent, file_size);
        }
        send(socketFd, response, strlen(response), 0);
    }
    else
    {
        strcpy(response, "Erreur: Le client n'est pas prêt à recevoir le fichier.\n");
        send(socketFd, response, strlen(response), 0);
    }
}

void handle_login(int client_socket)
{
    char buffer[200];
    char username[100];
    char password[100];
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    getpeername(client_socket, (struct sockaddr *)&addr, &len);
    send(client_socket, "Entrez votre pseudo: ", 22, 0);

    int rlen = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (rlen <= 0)
        return;

    buffer[rlen] = '\0';
    strncpy(username, buffer, sizeof(username));
    username[sizeof(username) - 1] = '\0';

    send(client_socket, "Entrez votre mot de passe: ", 28, 0);
    rlen = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (rlen <= 0)
        return;

    buffer[rlen] = '\0';
    strncpy(password, buffer, sizeof(password));
    password[sizeof(password) - 1] = '\0';

    User *user = findUserByName(username);

    if (!user)
    {
        User *newUser = createUser(client_socket, username, password, USER, &addr, true);

        // Vérifier si la création de l'utilisateur a réussi
        if (newUser == NULL)
        {
            fprintf(stderr, "Error: Failed to create new user\n");
            send(client_socket, "Erreur serveur: impossible de créer l'utilisateur.\n", 52, 0);
            close(client_socket);
            return;
        }

        add_client(newUser);
        addUserToList(global_users, newUser);
        send(client_socket, "Compte créé et connecté avec succès.\n", 39, 0);
        saveUsersToJson("users.json");
    }
    else if (strcmp(user->password, password) == 0)
    {
        user->authenticated = true;
        user->socket_fd = client_socket;
        user->ad = addr;
        add_client(user);
        send(client_socket, "Connexion réussie.\n", 20, 0);
    }
    else
    {
        send(client_socket, "Mot de passe incorrect.\n", 25, 0);
        close(client_socket);
    }
}

void sendAllClients(const char *message)
{
    sendInfoToAll((char *)message);
}

int main()
{
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);

    // Initialize our new structures with mutexes
    global_users = createUserList();

    loadUsersFromJson("users.json");
    initChannelSystem(); // Initialize channel system

    if (server_socket == -1)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY;
    ad.sin_port = htons((short)31473);

    int res = bind(server_socket, (struct sockaddr *)&ad, sizeof(ad));
    if (res == -1)
    {
        perror("bind");
        exit(1);
    }

    res = listen(server_socket, 5);
    if (res == -1)
    {
        perror("listen");
        exit(1);
    }

    client_sockets = createList(NULL);

    if (client_sockets == NULL)
    {
        perror("malloc client_sockets");
        exit(1);
    }

    printf("Serveur démarré sur le port 31473\n");

    while (!shouldShutdown) // Check the shutdown flag in the main loop
    {
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);

        // Use select with a timeout to periodically check shouldShutdown flag
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 1; // Check the flag every second
        timeout.tv_usec = 0;

        if (select(server_socket + 1, &readfds, NULL, NULL, &timeout) > 0)
        {
            int new_socket = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);

            if (new_socket < 0)
            {
                perror("accept");
                continue;
            }

            printf("Nouveau client connecté (socket: %d)\n", new_socket);

            int *arg = malloc(sizeof(int));
            if (arg == NULL)
            {
                perror("malloc");
                close(new_socket);
                continue;
            }

            *arg = new_socket;
            pthread_t thread_id;

            if (pthread_create(&thread_id, NULL, handle_client, arg) != 0)
            {
                perror("pthread_create");
                free(arg);
                remove_client(new_socket);
                continue;
            }
            pthread_detach(thread_id);
        }
    }

    printf("Server is shutting down...\n");

    sendAllClients("Server is shutting down. Goodbye!");

    sleep(1);

    if (client_sockets != NULL)
    {
        lockList(client_sockets);
        Node *current = client_sockets->first;
        while (current != NULL)
        {
            if (current->user != NULL)
            {
                close(current->user->socket_fd);
            }
            current = current->next;
        }
        unlockList(client_sockets);

        free(client_sockets);
    }

    close(server_socket);
    destroyUserList(global_users);
    cleanupChannelSystem();

    printf("Server shutdown complete\n");
    return 0;
}