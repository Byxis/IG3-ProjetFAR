#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "ChainedList.h"
#include "command.h"
#include "user.h"
#include "file.h"
#include "cJSON.h"

#define MAX_MESSAGE_SIZE 2000
#define MAX_CLIENTS 10

User *registered_users = NULL; 
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

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
    int shutdown_flag = 0;

    while (1)
    {
        int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        if (received <= 0)
        {
            break;
        }

        buffer[received] = '\0';
        printf("Client %d: %s\n", sock, buffer);

        executeCommand(sock, buffer, shutdown_flag);
        if (shutdown_flag)
        {
            break;
        }
    }

    printf("Client %d déconnecté\n", sock);
    removeClient(sock);
    return NULL;
}

User *findUserByName(const char *name)
{
    pthread_mutex_lock(&users_mutex);
    User *current = registered_users;
    while (current != NULL)
    {
        if (strcmp(current->name, name) == 0)
        {
            pthread_mutex_unlock(&users_mutex);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&users_mutex);
    return NULL;
}

Role getRoleByName(const char *name)
{
    User *user = findUserByName(name);
    if (user != NULL)
    {
        return user->role;
    }
    return USER; 
}

Role stringToRole(const char *roleStr) {
    if (strcmp(roleStr, "ADMIN") == 0) return ADMIN;
    return USER;
}

void loadUsersFromJson(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erreur ouverture JSON");
        return;
    }

    struct stat st;
    stat(filename, &st);
    char *data = malloc(st.st_size + 1);
    fread(data, 1, st.st_size, file);
    data[st.st_size] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(data);
    if (!json) {
        fprintf(stderr, "Erreur parsing JSON\n");
        free(data);
        return;
    }

    int count = cJSON_GetArraySize(json);
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        const char *username = cJSON_GetObjectItem(item, "username")->valuestring;
        const char *password = cJSON_GetObjectItem(item, "password")->valuestring;
        const char *role = cJSON_GetObjectItem(item, "role")->valuestring;

        User *new_user = malloc(sizeof(User));
        strcpy(new_user->name, username);
        strcpy(new_user->password, password);
        new_user->role = stringToRole(role);
        new_user->authenticated = false;
        new_user->next = registered_users;
        registered_users = new_user;
    }

    cJSON_Delete(json);
    free(data);
}
