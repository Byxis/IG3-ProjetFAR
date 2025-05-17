#ifndef USER_H
#define USER_H

#include <netinet/in.h>
#include <stdbool.h>

extern List *client_sockets;
extern pthread_mutex_t clients_mutex;

typedef enum
{
    USER,
    ADMIN
} Role;

typedef struct user
{
    char name[50];
    char password[50];
    Role role;
    struct sockaddr_in ad;
    int socket_fd;
    bool authenticated;
    struct user *next;
} User;

void sendAllClients(const char *message);
void sendClient(int socket_fd, const char *message);
void removeClient(int socket_fd);
void addClient(int sock);
void *handleClient(void *arg);
User *findUserByName(const char *name);
Role getRoleByName(const char *name);
void registerUser(const char *pseudo, const char *password, int socket_fd, struct sockaddr_in ad);
void saveUsersToJson(const char *filename);
void loadUsersFromJson(const char *filename);
User *findUserBySocket(int sock);

#endif
