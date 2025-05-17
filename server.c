#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "ChainedList.h"

#define MAX_MESSAGE_SIZE 2000
#define MAX_CLIENTS 10

List *client_sockets;

// Function prototypes
void sendAllClients(const char *message);
void sendClient(int socket_fd, const char *message);
void removeClient(int socket_fd);
void addClient(int sock);
void *handleClient(void *arg);

void addClient(int sock)
{
    addLast(client_sockets, sock);
}

void removeClient(int socket_fd)
{

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
}

void sendClient(int socket_fd, const char *message)
{

    if (isListEmpty(client_sockets))
    {
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
}

void sendAllClients(const char *message)
{

    if (isListEmpty(client_sockets))
    {
        return;
    }

    Node *current = client_sockets->first;
    while (current != NULL)
    {
        send(current->val, message, strlen(message) + 1, 0);
        current = current->next;
    }
}

void *handleClient(void *arg)
{
    int sock = *((int *)arg);
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
        sprintf(broadcast, "Client %d: %s", sock, buffer);
        printf("Client %d: %s\n", sock, buffer);
        sendAllClients(broadcast);
    }

    printf("Client %d déconnecté\n", sock);
    removeClient(sock);
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

    deleteList(client_sockets);
    free(client_sockets);
    close(dSock);
    return 0;
}