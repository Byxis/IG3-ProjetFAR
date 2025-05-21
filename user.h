#ifndef USER_H
#define USER_H

#include <netinet/in.h>
#include <stdbool.h>
#include "ChainedList.h"

extern List *client_sockets;

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

// Add user list structure with mutex
typedef struct
{
    User *head;
    pthread_mutex_t mutex;
} UserList;

// Declare global users list
extern UserList *global_users;

void *handleClient(void *arg);
void registerUser(const char *pseudo, const char *password, int socket_fd, struct sockaddr_in ad);
void saveUsersToFile(const char *filename);
void loadUsersFromFile(const char *filename);
User *createUser(int socketId, const char *name, const char *password, Role role, struct sockaddr_in *addr, bool authenticated);

// Add new functions for thread-safe user operations
UserList *createUserList();
void destroyUserList(UserList *list);
void addUserToList(UserList *list, User *user);
User *findUserByNameSafe(UserList *list, const char *username);

#endif
