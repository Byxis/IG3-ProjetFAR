#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define MAX_MESSAGE_SIZE 2000
#define MAX_CLIENTS 10

int client_sockets[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Function prototypes
void send_to_all_clients(const char *message);
void send_to_client(int client_index, const char *message);
void remove_client(int index);
int add_client(int sock);

// Ajouter un nouveau client
int add_client(int sock)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets[i] == -1)
        {
            client_sockets[i] = sock;
            pthread_mutex_unlock(&clients_mutex);
            return i; // Retourne l'index du client
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return -1; // Pas de place disponible
}

// Supprimer un client
void remove_client(int index)
{
    if (index >= 0 && index < MAX_CLIENTS)
    {
        pthread_mutex_lock(&clients_mutex);
        if (client_sockets[index] != -1)
        {
            close(client_sockets[index]);
            client_sockets[index] = -1;
        }
        pthread_mutex_unlock(&clients_mutex);
    }
}

// Envoyer un message à un client spécifique
void send_to_client(int client_index, const char *message)
{
    pthread_mutex_lock(&clients_mutex);
    if (client_index >= 0 && client_index < MAX_CLIENTS && client_sockets[client_index] != -1)
    {
        send(client_sockets[client_index], message, strlen(message) + 1, 0);
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Envoyer un message à tous les clients
void send_to_all_clients(const char *message)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets[i] != -1)
        {
            send(client_sockets[i], message, strlen(message) + 1, 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{
    int client_index = *((int *)arg);
    int sock = client_sockets[client_index];
    free(arg);

    char buffer[MAX_MESSAGE_SIZE];
    char broadcast[MAX_MESSAGE_SIZE + 20];

    while (1)
    {
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0)
        {
            break;
        }

        buffer[received] = '\0';
        printf("Message du client %d: %s\n", client_index, buffer);

        // Exemple d'utilisation: diffuser le message à tous
        sprintf(broadcast, "Client %d dit: %s", client_index, buffer);
        send_to_all_clients(broadcast);

        // Exemple d'utilisation: répondre uniquement à l'expéditeur
        sprintf(broadcast, "Message reçu: %s", buffer);
        send_to_client(client_index, broadcast);
    }

    printf("Client %d déconnecté\n", client_index);
    remove_client(client_index);
    return NULL;
}

int main()
{
    int dSock = socket(PF_INET, SOCK_STREAM, 0); // TCP socket creation
    if (dSock == -1)
    {
        perror("socket");
        exit(1);
    }

    // Permettre la réutilisation de l'adresse
    int opt = 1;
    if (setsockopt(dSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    struct sockaddr_in ad;
    ad.sin_family = AF_INET;
    ad.sin_addr.s_addr = INADDR_ANY;
    ad.sin_port = htons((short)31473);

    int res = bind(dSock, (struct sockaddr *)&ad, sizeof(ad));
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

    // Initialize client sockets array
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_sockets[i] = -1;
    }

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

        int client_index = add_client(new_socket);
        if (client_index == -1)
        {
            printf("Trop de clients, connexion refusée\n");
            close(new_socket);
            continue;
        }

        printf("Nouveau client connecté (index: %d)\n", client_index);

        int *arg = malloc(sizeof(int));
        if (arg == NULL)
        {
            perror("malloc");
            remove_client(client_index);
            continue;
        }
        *arg = client_index;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, arg) != 0)
        {
            perror("pthread_create");
            free(arg);
            remove_client(client_index);
            continue;
        }

        pthread_detach(thread_id);
    }

    return 0;
}