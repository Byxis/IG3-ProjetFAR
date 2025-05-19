#include "Channel.h"
#include "user.h" // Add this include to get the full User definition
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <pthread.h>

#define MESSAGE_BUFFER_SIZE 256
#define MESSAGE_BUFFER_COUNT 10

static char messageBuffers[MESSAGE_BUFFER_COUNT][MESSAGE_BUFFER_SIZE];
static int currentBufferIndex = 0;

// Initialize global channelList
ChannelList channelList = {NULL, PTHREAD_MUTEX_INITIALIZER};

// Mutex helper functions
void lockChannelList()
{
    pthread_mutex_lock(&channelList.mutex);
}

void unlockChannelList()
{
    pthread_mutex_unlock(&channelList.mutex);
}

static Channel *getChannel(char *name);
void removeChannel(char *name);
static Channel *createChannel(char *name, int maxSize, User *user);

/**
 ** Get a buffer for formatted messages.
 * This function uses a circular buffer to reuse message buffers.
 * @return {char*} buffer - A pointer to a buffer for formatted messages.
 */
static char *getMessageBuffer()
{
    char *buffer = messageBuffers[currentBufferIndex];
    currentBufferIndex = (currentBufferIndex + 1) % MESSAGE_BUFFER_COUNT;
    return buffer;
}

/**
 ** Format a message using a format string and variable arguments.
 * This function uses a circular buffer to reuse message buffers.
 * @param {const char*} format - The format string.
 * @param {...} - The variable arguments to format the message.
 * @return {char*} buffer - A pointer to the formatted message.
 */
static char *__attribute__((unused)) formatMessage(const char *format, ...)
{
    char *buffer = getMessageBuffer();
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MESSAGE_BUFFER_SIZE, format, args);
    va_end(args);
    return buffer;
}

/**
 ** Send a formatted message to all members of a channel.
 * This function uses a circular buffer to reuse message buffers.
 * @param {char*} channelName - The name of the channel.
 * @param {const char*} format - The format string.
 * @param {...} - The variable arguments to format the message.
 */
static void sendFormattedChannelMessage(char *channelName, const char *format, ...)
{
    char *buffer = getMessageBuffer();
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, MESSAGE_BUFFER_SIZE, format, args);
    va_end(args);
    sendToAllNamedChannelMembers(channelName, buffer);
}

/**
 ** Initialize the channel system.
 * This function creates the Hub channel and initializes the channel list.
 */
void initChannelSystem()
{
    lockChannelList();

    Channel *channel = (Channel *)malloc(sizeof(Channel));
    if (channel == NULL)
    {
        perror("Failed to allocate memory for channel");
        unlockChannelList();
        exit(EXIT_FAILURE);
    }
    channel->name = strdup("Hub");
    channel->maxSize = -1;
    channel->clients = createList(NULL);
    channel->next = NULL;
    channelList.first = channel;

    unlockChannelList();
}

/**
 ** Cleanup the channel system.
 * This function frees all memory associated with the channels and their clients.
 */
void cleanupChannelSystem()
{
    lockChannelList();

    Channel *current = channelList.first;
    Channel *next;

    while (current != NULL)
    {
        next = current->next;

        List *list = current->clients;
        Node *node = list->first;
        while (node != NULL)
        {
            Node *temp = node;
            node = node->next;
            free(temp);
        }
        free(list);

        free(current->name);
        free(current);
        current = next;
    }
    channelList.first = NULL;

    unlockChannelList();
    pthread_mutex_destroy(&channelList.mutex);
}

/**
 ** Create a new channel, with the specified name and maximum size.
 * @param {char*} {char *} name - The name of the channel.
 * @param {int} maxSize - The maximum size of the channel (-1 for unlimited).
 * @param {User *} user - The user creating the channel.
 * @return {Channel *} newChannel - A pointer to the newly created channel, or NULL on failure.
 */
static Channel *createChannel(char *name, int maxSize, User *user)
{
    if (name == NULL || strlen(name) == 0)
    {
        fprintf(stderr, "Channel name cannot be empty\n");
        return NULL;
    }

    Channel *existingChannel = getChannel(name);
    if (existingChannel != NULL)
    {
        fprintf(stderr, "Channel already exists\n");
        return NULL;
    }

    if (maxSize < -1)
    {
        fprintf(stderr, "Invalid max size for channel\n");
        return NULL;
    }
    lockChannelList();
    Channel *newChannel = (Channel *)malloc(sizeof(Channel));
    if (newChannel == NULL)
    {
        perror("Failed to allocate memory for new channel");
        unlockChannelList();
        return NULL;
    }
    newChannel->name = strdup(name);
    newChannel->maxSize = maxSize;
    newChannel->clients = createList(user);
    newChannel->next = channelList.first->next;
    channelList.first->next = newChannel;
    unlockChannelList();
    return newChannel;
}

/**
 ** Get a channel by its name.
 * @param {char *} name - The name of the channel.
 * @return {Channel *} channel - A pointer to the channel, or NULL if not found.
 */
static Channel *getChannel(char *name)
{
    lockChannelList();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            unlockChannelList();
            return channel;
        }
        channel = channel->next;
    }
    unlockChannelList();
    return NULL;
}

/**
 ** Check if a channel is full.
 * @param {char *} name - The name of the channel.
 * @return {bool} - true if the channel is full, false otherwise.
 */
static bool isChannelFull(char *name)
{
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        return false;
    }
    if (channel->maxSize == -1)
    {
        return false;
    }

    bool result = channel->clients->size > channel->maxSize;
    return result;
}

/**
 ** Get the size of a channel.
 * @param {char *} name - The name of the channel.
 * @return {int} - The size of the channel, or -1 if not found.
 */
static int getChannelSize(char *name)
{
    Channel *channel = getChannel(name);
    lockChannelList();
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        unlockChannelList();
        return -1;
    }
    int size;
    if (strcmp(channel->name, "Hub") == 0)
    {
        size = channel->clients->size - 1;
    }
    else
    {
        size = channel->clients->size;
    }
    unlockChannelList();
    return size;
}

/**
 ** Check if a channel is empty.
 * @param {char *} name - The name of the channel.
 * @return {bool} - true if the channel is empty, false otherwise.
 */
static bool __attribute__((unused)) isChannelEmpty(char *name)
{
    Channel *channel = getChannel(name);
    lockChannelList();
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        unlockChannelList();
        return false;
    }
    if (isListEmpty(channel->clients))
    {
        fprintf(stderr, "Channel is empty\n");
        unlockChannelList();
        return true;
    }
    fprintf(stderr, "Channel is not empty\n");
    unlockChannelList();
    return false;
}

/**
 ** Get the name of the channel a user is in.
 * @param user The user
 * @return {char *} The name of the channel, or NULL if not found.
 */
char *getUserChannelName(User *user)
{
    Channel *channel = getUserChannel(user);
    if (channel != NULL)
    {
        char *name = channel->name;
        return name;
    }
    return NULL;
}

/**
 ** Get the channel a user is in.
 * @param user The user
 * @return {Channel *} A pointer to the channel, or NULL if not found.
 */
Channel *getUserChannel(User *user)
{
    lockChannelList();
    if (user == NULL)
    {
        unlockChannelList();
        return NULL;
    }

    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        Node *current = channel->clients->first;
        while (current != NULL)
        {
            if (current->user != NULL && current->user == user)
            {
                unlockChannelList();
                return channel;
            }
            current = current->next;
        }
        channel = channel->next;
    }
    unlockChannelList();
    return NULL;
}

/**
 ** Add a user to a specified channel name.
 * @param {char *} name - The name of the channel.
 * @param {User *} user - The user to add.
 */
void addUserToChannel(char *name, User *user)
{
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        exit(EXIT_FAILURE);
    }
    if (isChannelFull(name))
    {
        fprintf(stderr, "Channel is full, cannot add user\n");
        return;
    }
    addLast(channel->clients, user);
    return;
}

/**
 ** Remove a user from a specified channel name.
 * @param {char *} name - The name of the channel.
 * @param {User *} user - The user to remove.
 */
void removeUserFromChannel(char *name, User *user)
{
    lockChannelList();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            removeElement(channel->clients, user);
            unlockChannelList();
            return;
        }
        channel = channel->next;
    }
    unlockChannelList();
}

/**
 ** Remove a channel by its name.
 * @param {char *} name - The name of the channel.
 */
void removeChannel(char *name)
{
    if (name == NULL || strlen(name) == 0)
    {
        fprintf(stderr, "Channel name cannot be empty\n");
        return;
    }

    if (strcmp(channelList.first->name, name) == 0)
    {
        fprintf(stderr, "Cannot remove the hub channel\n");
        return;
    }
    lockChannelList();
    Channel *current = channelList.first;
    Channel *previous = NULL;

    while (current != NULL)
    {
        if (strcmp(current->name, name) == 0)
        {
            if (previous == NULL)
            {
                channelList.first = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            deleteList(current->clients);
            free(current->name);
            free(current);
            unlockChannelList();
            return;
        }
        previous = current;
        current = current->next;
    }
    unlockChannelList();
}

/**
 ** Send a message to all users in the channel of the specified user.
 * @param {User *} user - The user.
 * @param {const char*} message - The message to send.
 */
void sendUserChannelMessage(User *user, const char *message)
{
    Channel *channel = getUserChannel(user);
    if (channel == NULL)
    {
        fprintf(stderr, "User is not in any channel\n");
        return;
    }
    lockChannelList();
    Node *current = channel->clients->first;
    while (current != NULL)
    {
        if (current->user != NULL)
        {
            send(current->user->socket_fd, message, strlen(message) + 1, 0);
        }
        current = current->next;
    }
    unlockChannelList();
}

/**
 ** Leave the channel of the specified user.
 * @param {User *} user - The user.
 * @param {char*} response - The response message.
 * @param {size_t} responseSize - The size of the response buffer.
 * @return {bool} - true if successful, false otherwise.
 */
bool userLeaveChannel(User *user, char *response, size_t responseSize)
{
    Channel *userChannel = getUserChannel(user);
    if (userChannel == NULL)
    {
        snprintf(response, responseSize, "You are not in any channel");
        return false;
    }

    char *name = userChannel->name;
    if (strcmp(name, "Hub") == 0)
    {
        snprintf(response, responseSize, "You cannot leave the Hub channel");
        return false;
    }

    sendFormattedChannelMessage(name, "%s has left the channel %s (%d/%d)",
                                user->name, name, getChannelSize(name) - 1, userChannel->maxSize);

    sendFormattedChannelMessage("Hub", "%s has joined the channel %s (%d/%d)",
                                user->name, "Hub", getChannelSize("Hub"), -1);

    removeUserFromChannel(name, user);
    addUserToChannel("Hub", user);
    return true;
}

/**
 ** Create and join a new channel.
 * @param {char*} name - The name of the channel.
 * @param {int} maxSize - The maximum size of the channel (-1 for unlimited).
 * @param {User *} user - The user creating and joining the channel.
 * @param {char*} response - The response message.
 * @param {size_t} responseSize - The size of the response buffer.
 * @return {bool} - true if successful, false otherwise.
 */
bool userCreateAndJoinChannel(char *name, int maxSize, User *user, char *response, size_t responseSize)
{
    if (name == NULL || strlen(name) == 0)
    {
        snprintf(response, responseSize, "Channel name cannot be empty");
        return false;
    }

    char *currentChannel = getUserChannelName(user);

    if (currentChannel != NULL && strcmp(currentChannel, name) != 0)
    {
        sendFormattedChannelMessage(currentChannel, "%s has created a new channel named %s (%d/%d)",
                                    user->name, name, 1, maxSize);

        removeUserFromChannel(currentChannel, user);
    }
    printf("Creating channel:::  %s\n", name);

    Channel *newChannel = createChannel(name, maxSize, user);

    printf("Channel created:::  %s\n", name);
    fflush(stdout);

    // Affichage de la liste des canaux
    Channel *current = channelList.first;
    while (current != NULL)
    {
        printf("Channel: %s, Size: %d\n", current->name, current->clients->size);
        current = current->next;
    }

    if (newChannel == NULL)
    {
        snprintf(response, responseSize, "Failed to create channel %s", name);
        return false;
    }

    int currentSize = getChannelSize(name);
    snprintf(response, responseSize, "You have created and joined %s (%d/%d)", name, currentSize, maxSize);
    return true;
}

/**
 ** Join an existing channel.
 * @param {char*} name - The name of the channel.
 * @param {User *} user - The user joining the channel.
 * @param {char*} response - The response message.
 * @param {size_t} responseSize - The size of the response buffer.
 * @return {bool} - true if successful, false otherwise.
 */
bool userJoinChannel(char *name, User *user, char *response, size_t responseSize)
{
    if (name == NULL || strlen(name) == 0)
    {
        snprintf(response, responseSize, "Channel name cannot be empty");
        return false;
    }

    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        snprintf(response, responseSize, "Channel %s does not exist", name);
        return false;
    }

    if (isChannelFull(name))
    {
        snprintf(response, responseSize, "This channel is full, you cannot join it");
        return false;
    }

    Channel *userChannel = getUserChannel(user);
    if (strcmp(userChannel->name, name) == 0)
    {
        snprintf(response, responseSize, "You are already in this channel");
        return false;
    }

    Channel *currentChannel = getUserChannel(user);
    if (currentChannel != NULL)
    {
        removeUserFromChannel(currentChannel->name, user);
    }

    addUserToChannel(name, user);

    int currentSize = getChannelSize(name);
    snprintf(response, responseSize, "You have joined %s (%d/%d)", name, currentSize, channel->maxSize);

    sendFormattedChannelMessage(name, "%s has joined the channel %s (%d/%d)",
                                user->name, name, currentSize, channel->maxSize);

    sendFormattedChannelMessage(currentChannel->name, "%s has left the channel %s (%d/%d)",
                                user->name, currentChannel->name,
                                getChannelSize(currentChannel->name), currentChannel->maxSize);

    return true;
}

/**
 ** Send a message to all members of the channel of the specified user.
 * @param {User *} user - The user.
 * @param {const char*} message - The message to send.
 */
void sendToAllUserChannelMembers(User *user, const char *message)
{
    lockChannelList();
    Channel *channel = getUserChannel(user);
    if (channel == NULL)
    {
        fprintf(stderr, "User is not in any channel\n");
        unlockChannelList();
        return;
    }

    Node *current = channel->clients->first;
    while (current != NULL)
    {
        if (current->user != NULL)
        {
            send(current->user->socket_fd, message, strlen(message) + 1, 0);
        }
        current = current->next;
    }
    unlockChannelList();
}

/**
 ** Send a message to all members of a specified channel, except the sender.
 * @param {char*} name - The name of the channel.
 * @param {const char*} message - The message to send.
 * @param {User *} user - The sender.
 */
void sendToAllNamedChannelMembersExceptUser(char *name, const char *message, User *user)
{
    Channel *channel = getChannel(name);
    lockChannelList();
    if (channel == NULL)
    {
        fprintf(stderr, "Channel %s does not exist\n");
        unlockChannelList();
        return;
    }

    Node *current = channel->clients->first;
    while (current != NULL)
    {
        if (current->user != NULL && current->user != user)
        {
            send(current->user->socket_fd, message, strlen(message) + 1, 0);
        }
        current = current->next;
    }
    unlockChannelList();
}

/**
 ** Send a message to all users in all channels, except the sender.
 * @param {User *} user - The sender.
 * @param {char*} message - The message to send.
 */
void sendInfoToAllExceptUser(User *user, char *message)
{
    lockChannelList();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        Node *current = channel->clients->first;
        while (current != NULL)
        {
            if (current->user != NULL && current->user != user)
            {
                send(current->user->socket_fd, message, strlen(message) + 1, 0);
            }
            current = current->next;
        }
        channel = channel->next;
    }
    unlockChannelList();
}

/**
 ** Register a user and add them to the Hub channel.
 * @param {User*} user - The user to register.
 * @returns {bool} - true if successful, false otherwise.
 */
bool registerUserInHub(User *user)
{
    if (user == NULL)
        return false;

    printf("Registering user %s in Hub channel\n", user->name);
    printf("Registering after lock user %s in Hub channel\n", user->name);
    Channel *hub = getChannel("Hub");
    if (hub == NULL)
    {
        printf("Hub channel does not exist\n");
        return false;
    }
    printf("Adding user %s to Hub channel\n", user->name);

    addLast(hub->clients, user);

    printf("User %s added to Hub channel\n", user->name);

    sendFormattedChannelMessage("Hub", "%s has joined the server", user->name);

    return true;
}

/**
 ** Disconnect a user from all channels.
 * This should be called when a user disconnects from the server.
 * @param {User*} user - The user to disconnect.
 */
void disconnectUserFromAllChannels(User *user)
{
    if (user == NULL)
        return;

    lockChannelList();
    // First find which channel the user is in
    Channel *userChannel = NULL;
    Channel *current = channelList.first;

    while (current != NULL)
    {
        Node *node = current->clients->first;
        while (node != NULL)
        {
            if (node->user != NULL && node->user == user)
            {
                userChannel = current;
                break;
            }
            node = node->next;
        }
        if (userChannel)
            break;
        current = current->next;
    }

    if (userChannel)
    {
        char channelName[100];
        strcpy(channelName, userChannel->name);
        removeElement(userChannel->clients, user);
        sendFormattedChannelMessage(channelName, "%s has disconnected from the server", user->name);
    }
    unlockChannelList();
}

/**
 ** Find a user by their socket ID across all channels.
 * @param {int} socketId - The socket ID to search for.
 * @returns {User*} - Pointer to the found user or NULL if not found.
 */
User *findUserBySocketId(int socketId)
{
    User *foundUser = NULL;

    lockChannelList();
    Channel *current = channelList.first;

    while (current != NULL)
    {
        Node *node = current->clients->first;
        while (node != NULL)
        {
            if (node->user != NULL && node->user->socket_fd == socketId)
            {
                foundUser = node->user;
                unlockChannelList();
                return foundUser;
            }
            node = node->next;
        }
        current = current->next;
    }

    unlockChannelList();
    return NULL;
}

/**
 ** Get a list of all users in a channel.
 * @param {char*} channelName - The name of the channel.
 * @param {char*} result - Buffer to store the user list.
 * @param {size_t} size - Size of the result buffer.
 * @returns {bool} - true if successful, false otherwise.
 */
bool getUsersInChannel(char *channelName, char *result, size_t size)
{
    if (!channelName || !result)
        return false;

    Channel *channel = getChannel(channelName);
    lockChannelList();
    if (!channel)
    {
        unlockChannelList();
        snprintf(result, size, "Channel %s does not exist", channelName);
        return false;
    }

    // Format: Channel 'name' (x/y users): user1, user2, user3
    int count = 0;
    int maxSize = channel->maxSize;
    char maxSizeStr[20];
    if (maxSize == -1)
    {
        strcpy(maxSizeStr, "unlimited");
    }
    else
    {
        snprintf(maxSizeStr, sizeof(maxSizeStr), "%d", maxSize);
    }

    size_t written = (size_t)snprintf(result, size, "Channel '%s' (%d/%s users): ",
                                      channelName, channel->clients->size, maxSizeStr);

    Node *node = channel->clients->first;
    while (node != NULL && written < size)
    {
        if (count > 0)
        {
            written += snprintf(result + written, size - written, ", ");
        }
        written += snprintf(result + written, size - written, "%s", node->user->name);
        count++;
        node = node->next;
    }

    unlockChannelList();
    return true;
}

/**
 ** Get a list of all channels.
 * @param {char*} result - Buffer to store the channel list.
 * @param {size_t} size - Size of the result buffer.
 * @returns {bool} - true if successful, false otherwise.
 */
bool listAllChannels(char *result, size_t size)
{
    if (!result)
        return false;

    lockChannelList();
    size_t written = (size_t)snprintf(result, size, "Available channels:\n");

    Channel *channel = channelList.first;
    while (channel != NULL && written < size)
    {
        char maxSizeStr[20];
        if (channel->maxSize == -1)
        {
            strcpy(maxSizeStr, "unlimited");
        }
        else
        {
            snprintf(maxSizeStr, sizeof(maxSizeStr), "%d", channel->maxSize);
        }

        written += snprintf(result + written, size - written,
                            "- %s (%d/%s users)\n",
                            channel->name, channel->clients->size, maxSizeStr);

        channel = channel->next;
    }

    unlockChannelList();
    return true;
}

/**
 ** Send a message to all members of a specified channel.
 * @param {char*} name - The name of the channel.
 * @param {const char*} message - The message to send.
 */
void sendToAllNamedChannelMembers(char *name, const char *message)
{
    printf("Sending message to all members of channel %s: %s\n", name, message);
    Channel *channel = channelList.first;
    lockChannelList();
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            Node *current = channel->clients->first;
            while (current != NULL)
            {
                printf("Sending message to user %s\n", current->user->name);
                if (current->user != NULL)
                {
                    send(current->user->socket_fd, message, strlen(message) + 1, 0);
                }
                current = current->next;
            }
            unlockChannelList();
            return;
        }
        channel = channel->next;
    }
    unlockChannelList();
    fprintf(stderr, "Channel %s does not exist\n", name);
}

/**
 ** Send a message to all clients in all channels.
 * @param {char*} message - The message to send.
 */
void sendInfoToAll(char *message)
{
    lockChannelList();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        Node *current = channel->clients->first;
        while (current != NULL)
        {
            if (current->user != NULL)
            {
                send(current->user->socket_fd, message, strlen(message) + 1, 0);
            }
            current = current->next;
        }
        channel = channel->next;
    }
    unlockChannelList();
}