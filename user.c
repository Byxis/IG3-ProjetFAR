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
#include "cJSON.h"

#define MAX_CLIENTS 10

User *registered_users = NULL;
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 ** Finds a user by their username.
 * @param name (const char*) - The username to search for.
 * @returns User* - Pointer to the user struct, or NULL if not found.
 */
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

/**
 ** Finds a user by their socket file descriptor.
 * @param sock (int) - The socket file descriptor to search for.
 * @returns User* - Pointer to the user struct, or NULL if not found.
 */
User *findUserBySocket(int sock)
{
    pthread_mutex_lock(&users_mutex);
    User *current = registered_users;
    while (current != NULL)
    {
        if (current->socket_fd == sock)
        {
            pthread_mutex_unlock(&users_mutex);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&users_mutex);
    return NULL;
}

/**
 ** Gets the role of a user by their username.
 * @param name (const char*) - The username to search for.
 * @returns Role - The user's role (ADMIN or USER).
 */
Role getRoleByName(const char *name)
{
    User *user = findUserByName(name);
    if (user != NULL)
    {
        return user->role;
    }
    return USER;
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
 ** Loads users from a JSON file into memory.
 * @param filename (const char*) - The path to the JSON file.
 * @returns void
 */
void loadUsersFromJson(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
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

    if (!json)
    {
        fprintf(stderr, "Erreur parsing JSON\n");
        free(data);
        return;
    }
    int count = cJSON_GetArraySize(json);
    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_GetArrayItem(json, i);
        const char *username = cJSON_GetObjectItem(item, "username")->valuestring;
        const char *password = cJSON_GetObjectItem(item, "password")->valuestring;
        const char *role = cJSON_GetObjectItem(item, "role")->valuestring;
        User *new_user = malloc(sizeof(User));

        strcpy(new_user->name, username);
        strcpy(new_user->password, password);

        new_user->role = stringToRole(role);
        new_user->authenticated = false;
        new_user->socket_fd = -1;
        new_user->next = registered_users;
        registered_users = new_user;
    }
    cJSON_Delete(json);
    free(data);
}

/**
 ** Saves all users in memory to a JSON file.
 * @param filename (const char*) - The path to the JSON file.
 * @returns void
 */
void saveUsersToJson(const char *filename)
{
    pthread_mutex_lock(&users_mutex);
    cJSON *json = cJSON_CreateArray();
    User *current = registered_users;

    while (current != NULL)
    {
        cJSON *user_obj = cJSON_CreateObject();

        cJSON_AddStringToObject(user_obj, "username", current->name);
        cJSON_AddStringToObject(user_obj, "password", current->password);
        cJSON_AddStringToObject(user_obj, "role", current->role == ADMIN ? "ADMIN" : "USER");
        cJSON_AddItemToArray(json, user_obj);

        current = current->next;
    }

    char *data = cJSON_Print(json);
    FILE *file = fopen(filename, "w");

    if (file)
    {
        fputs(data, file);
        fclose(file);
    }
    free(data);
    cJSON_Delete(json);
    pthread_mutex_unlock(&users_mutex);
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
    pthread_mutex_lock(&users_mutex);
    User *existing = registered_users;

    while (existing != NULL)
    {
        if (strcmp(existing->name, username) == 0)
        {
            pthread_mutex_unlock(&users_mutex);
            send(socketFd, "Utilisateur déjà enregistré.\n", 33, 0);
            return;
        }
        existing = existing->next;
    }
    User *new_user = malloc(sizeof(User));

    strcpy(new_user->name, username);
    strcpy(new_user->password, password);

    new_user->role = USER;
    new_user->ad = addr;
    new_user->socket_fd = socketFd;
    new_user->authenticated = true;
    new_user->next = registered_users;
    registered_users = new_user;

    pthread_mutex_unlock(&users_mutex);
    saveUsersToJson("users.json");
    send(socketFd, "Utilisateur enregistré avec succès.\n", 39, 0);
}
