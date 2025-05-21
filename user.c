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

extern UserList *global_users;

/**
 ** Create a new UserList structure with mutex initialized
 * @returns UserList* - Pointer to the new UserList
 */
UserList *createUserList()
{
    UserList *list = (UserList *)malloc(sizeof(UserList));
    if (list)
    {
        list->head = NULL;
        pthread_mutex_init(&list->mutex, NULL);
    }
    return list;
}

/**
 ** Destroy a UserList and free all resources
 * @param list (UserList*) - The list to destroy
 */
void destroyUserList(UserList *list)
{
    if (!list)
        return;

    pthread_mutex_lock(&list->mutex);

    User *current = list->head;
    User *next;

    while (current)
    {
        next = current->next;
        free(current);
        current = next;
    }

    pthread_mutex_unlock(&list->mutex);
    pthread_mutex_destroy(&list->mutex);
    free(list);
}

/**
 ** Add a user to the user list in a thread-safe manner
 * @param list (UserList*) - The user list
 * @param user (User*) - The user to add
 */
void addUserToList(UserList *list, User *user)
{
    if (!list || !user)
        return;

    pthread_mutex_lock(&list->mutex);

    user->next = list->head;
    list->head = user;

    pthread_mutex_unlock(&list->mutex);
}

/**
 ** Find a user by name in a thread-safe manner
 * @param list (UserList*) - The user list to search
 * @param username (const char*) - The username to find
 * @returns User* - The found user or NULL
 */
User *findUserByNameSafe(UserList *list, const char *username)
{
    if (!list || !username)
        return NULL;

    pthread_mutex_lock(&list->mutex);

    User *current = list->head;
    while (current)
    {
        if (strcmp(current->name, username) == 0)
        {
            pthread_mutex_unlock(&list->mutex);
            return current;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&list->mutex);
    return NULL;
}

/**
 ** Converts a string to a Role enum value.
 * @param roleStr (const char*) - The string representing the role.
 * @returns Role - The corresponding Role enum value.
 */
Role stringToRole(const char *roleStr)
{
    if (strcmp(roleStr, "ADMIN") == 0)
        return ADMIN;
    return USER;
}

/**
 ** Creates a new User object with all fields initialized.
 * @param socketId (int) - The socket ID for the user.
 * @param name (const char*) - The name for the user. If NULL, default to "Client" + socketId.
 * @param password (const char*) - The password for the user. If NULL, set empty.
 * @param role (Role) - The role for the user. Default is USER.
 * @param addr (struct sockaddr_in*) - The address for the user. If NULL, zeroed out.
 * @param authenticated (bool) - Whether the user is authenticated. Default is false.
 * @returns User* - Pointer to the newly created User object.
 */
User *createUser(int socketId, const char *name, const char *password,
                 Role role, struct sockaddr_in *addr, bool authenticated)
{
    User *newUser = (User *)malloc(sizeof(User));
    if (newUser == NULL)
    {
        return NULL;
    }

    if (name != NULL)
    {
        strncpy(newUser->name, name, sizeof(newUser->name) - 1);
        newUser->name[sizeof(newUser->name) - 1] = '\0';
    }
    else
    {
        sprintf(newUser->name, "Client%d", socketId);
    }

    if (password != NULL)
    {
        strncpy(newUser->password, password, sizeof(newUser->password) - 1);
        newUser->password[sizeof(newUser->password) - 1] = '\0';
    }
    else
    {
        newUser->password[0] = '\0';
    }

    newUser->role = role;
    newUser->socket_fd = socketId;

    if (addr != NULL)
    {
        newUser->ad = *addr;
    }
    else
    {
        memset(&newUser->ad, 0, sizeof(struct sockaddr_in));
    }
    newUser->authenticated = authenticated;

    return newUser;
}

/**
 ** Loads users from a text file into memory.
 * @param filename (const char*) - The path to the text file.
 * @returns void
 */
void loadUsersFromFile(const char *filename)
{
    FileHandler fileHandler = open_file(filename, "r");
    if (fileHandler.fp == NULL)
    {
        perror("Erreur ouverture fichier utilisateurs");
        return;
    }

    char buffer[256];
    while (read_file(&fileHandler, buffer, sizeof(buffer)))
    {
        char username[50];
        char password[50];
        char roleStr[10];
        
        if (sscanf(buffer, "%s %s %s", username, password, roleStr) == 3)
        {
            User *new_user = malloc(sizeof(User));
            if (new_user == NULL)
            {
                perror("Erreur allocation utilisateur");
                continue;
            }

            strcpy(new_user->name, username);
            strcpy(new_user->password, password);
            new_user->role = stringToRole(roleStr);
            new_user->authenticated = false;
            new_user->socket_fd = -1;

            addUserToList(global_users, new_user);
        }
    }

    close_file(&fileHandler);
}

/**
 ** Saves all users in memory to a text file.
 * @param filename (const char*) - The path to the text file.
 * @returns void
 */
void saveUsersToFile(const char *filename)
{
    FileHandler fileHandler = open_file(filename, "w");
    if (fileHandler.fp == NULL)
    {
        perror("Erreur ouverture fichier utilisateurs pour écriture");
        return;
    }

    pthread_mutex_lock(&global_users->mutex);
    
    User *current = global_users->head;
    while (current != NULL)
    {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s %s %s", 
                 current->name, 
                 current->password, 
                 current->role == ADMIN ? "ADMIN" : "USER");
        
        write_file(&fileHandler, buffer);
        current = current->next;
    }

    pthread_mutex_unlock(&global_users->mutex);
    close_file(&fileHandler);
}

/**
 ** Registers a new user and adds them to the in-memory list.
 * @param username (const char*) - The username to register.
 * @param password (const char*) - The password for the user.
 * @param socketFd (int) - The socket file descriptor for the user.
 * @param addr (struct sockaddr_in) - The user's address.
 * @returns void
 */
void registerUser(const char *username, const char *password, int socketFd, struct sockaddr_in addr)
{
    User *existing = findUserByNameSafe(global_users, username);

    if (existing != NULL)
    {
        send(socketFd, "Utilisateur déjà enregistré.\n", 33, 0);
        return;
    }

    User *new_user = malloc(sizeof(User));

    strcpy(new_user->name, username);
    strcpy(new_user->password, password);

    new_user->role = USER;
    new_user->ad = addr;
    new_user->socket_fd = socketFd;
    new_user->authenticated = true;
    addUserToList(global_users, new_user);

    saveUsersToFile("save_users.txt");
    send(socketFd, "Utilisateur enregistré avec succès.\n", 39, 0);
}
