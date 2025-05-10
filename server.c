#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "ChainedList.h"
#include "file.h"

#define MAX_MESSAGE_SIZE 2000
#define MAX_CLIENTS 10

List *client_sockets;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void sendAllClients(const char *message);
void sendClient(int socket_fd, const char *message);
void removeClient(int socket_fd);
void addClient(int sock);
void *handleClient(void *arg);
void sendFileContent(int client, const char *filename);
void upload(int sock, const char *filename);
void download(int sock, const char *filename);

void addClient(int sock)
{
    pthread_mutex_lock(&clients_mutex);
    addLast(client_sockets, sock);
    pthread_mutex_unlock(&clients_mutex);
}

void removeClient(int socket_fd)
{
    pthread_mutex_lock(&clients_mutex);

    if (!isListEmpty(client_sockets))
    {
        Node *current = client_sockets->first;
        while (current != NULL)
        {
            if (current->val == socket_fd)
            {
                close(socket_fd);
                break;
            }
            current = current->next;
        }
        removeElement(client_sockets, socket_fd);
    }

    pthread_mutex_unlock(&clients_mutex);
}

void sendClient(int socket_fd, const char *message)
{
    pthread_mutex_lock(&clients_mutex);

    if (isListEmpty(client_sockets))
    {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    Node *current = client_sockets->first;
    while (current != NULL)
    {
        if (current->val == socket_fd)
        {
            send(socket_fd, message, strlen(message) + 1, 0);
            break;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}

void sendAllClients(const char *message)
{
    pthread_mutex_lock(&clients_mutex);

    if (isListEmpty(client_sockets))
    {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    Node *current = client_sockets->first;
    while (current != NULL)
    {
        send(current->val, message, strlen(message) + 1, 0);
        current = current->next;
    }

    pthread_mutex_unlock(&clients_mutex);
}

void *handleClient(void *arg)
{
    int sock = *((int *)arg);
    free(arg);

    char buffer[MAX_MESSAGE_SIZE];
    char broadcast[MAX_MESSAGE_SIZE + 50];

    while (1)
    {
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0)
        {
            break;
        }

        buffer[received] = '\0';


        // Order processing
        if (strcmp(buffer, "@help") == 0) {
            sendFileContent(sock, "README.txt");
            continue;
        }

        if (strcmp(buffer, "@credits") == 0) {
            sendFileContent(sock, "Credits.txt");
            continue;
        }

        if (strncmp(buffer, "@upload", 7) == 0) {
            char filename[100];
            if (sscanf(buffer + 8, "%s", filename) == 1) {
                upload(sock, filename);
            }
            else {
                send(sock, "Nom de fichier manquant.\n", 26, 0);
            }
            continue;
        }

        if (strncmp(buffer, "@download", 9) == 0) {
            char filename[100];
            if (sscanf(buffer + 8, "%s", filename) == 1) {
                download(sock, filename);
            } else {
                send(sock, "Nom de fichier manquant.\n", 26, 0);
            }
            continue;
        }

        sprintf(broadcast, "Client %d: %s", sock, buffer);
        printf("Client %d: %s\n", sock, buffer);
        sendAllClients(broadcast);
    }



    printf("Client %d déconnecté\n", sock);
    removeClient(sock);
    return NULL;
}


void sendFileContent(int client, const char *filename) {
    FileHandler file = open_file(filename, "r");

    char ligne[1024];
    while (read_file(&file, ligne, sizeof(ligne)) == 1) {
        send(client, ligne, strlen(ligne), 0);
        send(client, "\n", 1, 0);
    }
    close_file(&file);
}

// reçoit un fichier envoyé par un client via le socket et le sauvegarde localement
void upload(int sock, const char *filename) {
    // sécurité: éviter les chemins relatifs (fichers dangereux)
    if (strstr(filename, "..") != NULL) {
        send(sock, "Nom de fichier invalide.\n", 26, 0);
        return;
    }
    FileHandler file = open_file(filename, "wb"); // ouvre le fichier pour écriture binaire
    char buffer[1024]; // pour stocker les données reçues du client
    int len;

    while ((len = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        if (strncmp(buffer, "__END__", 7) == 0 && len == 7) {// détection du marqueur de fin
            break;
        }
        fwrite(buffer, 1, len, file.fp);
    }
    close_file(&file);
    send(sock, "Fichier reçu avec succès\n", 26, 0);
}


// lit un fichier local en binaire et l’envoie au client via le socket
void download(int sock, const char *filename) {
    // sécurité : éviter les chemins relatifs (fichers dangereux)
    if (strstr(filename, "..") != NULL) {
        send(sock, "Nom de fichier invalide.\n", 26, 0);
        return;
    }
    FileHandler file = open_file(filename, "rb"); // ouverture du fichier en lecture binaire
    char buffer[1024]; // stock du blocs de fichiers lus
    size_t bytes; // stocke du nombre d'octets lus

    while ((bytes = fread(buffer, 1, sizeof(buffer), file.fp)) > 0) {
        send(sock, buffer, bytes, 0);
    }
    close_file(&file);
    send(sock, "__END__", 7, 0);
}



int main()
{
    int dSock = socket(PF_INET, SOCK_STREAM, 0); // TCP socket creation
    if (dSock == -1)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(dSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) // allow reuse of the address
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY;
    ad.sin_port = htons((short)31473);

    int res = bind(dSock, (struct sockaddr *)&ad, sizeof(ad)); // bind the socket to the address
    if (res == -1)
    {
        perror("bind");
        exit(1);
    }

    res = listen(dSock, 5); // listen on the socket
    if (res == -1)
    {
        perror("listen");
        exit(1);
    }

    client_sockets = (List *)malloc(sizeof(List));
    if (client_sockets == NULL)
    {
        perror("malloc client_sockets");
        exit(1);
    }
    client_sockets->first = NULL;
    client_sockets->curr = NULL;
    client_sockets->size = 0;

    printf("Serveur démarré sur le port 31473\n");

    while (1)
    {
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);

        int new_socket = accept(dSock, (struct sockaddr *)&clientaddr, &addrlen);

        if (new_socket < 0)
        {
            perror("accept");
            continue;
        }

        printf("Nouveau client connecté (socket: %d)\n", new_socket);
        addClient(new_socket);

        int *arg = malloc(sizeof(int));
        if (arg == NULL)
        {
            perror("malloc");
            removeClient(new_socket);
            continue;
        }
        *arg = new_socket;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleClient, arg) != 0)
        {
            perror("pthread_create");
            free(arg);
            removeClient(new_socket);
            continue;
        }
        pthread_detach(thread_id);
    }

    free(client_sockets);
    close(dSock);
    pthread_mutex_destroy(&clients_mutex);
    return 0;
}