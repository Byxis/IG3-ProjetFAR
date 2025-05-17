#include "Channel.h"
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

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct
{
    Channel *first;
} channelList = {NULL};

static Channel *getChannel(char *name);
static bool isChannelFull(char *name);
static int getChannelSize(char *name);
static bool isChannelEmpty(char *name);
static Channel *createChannel(char *name, int maxSize, int clientSocket);
static void removeChannel(char *name);
static void lock_mutex()
{
    pthread_mutex_lock(&clients_mutex);
}

static void unlock_mutex()
{
    pthread_mutex_unlock(&clients_mutex);
}

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
static char *formatMessage(const char *format, ...)
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
    lock_mutex();
    Channel *channel = (Channel *)malloc(sizeof(Channel));
    if (channel == NULL)
    {
        perror("Failed to allocate memory for channel");
        unlock_mutex();
        exit(EXIT_FAILURE);
    }
    channel->name = strdup("Hub");
    channel->maxSize = -1;
    channel->clients = createList(-1);
    channel->next = NULL;
    channelList.first = channel;
    unlock_mutex();
}

/**
 ** Cleanup the channel system.
 * This function frees all memory associated with the channels and their clients.
 */
void cleanupChannelSystem()
{
    lock_mutex();
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
    unlock_mutex();
    pthread_mutex_destroy(&clients_mutex);
}

/**
 ** Create a new channel, with the specified name and maximum size.
 * @param {char*} {char *} name - The name of the channel.
 * @param {int} maxSize - The maximum size of the channel (-1 for unlimited).
 * @param {int} clientSocket - The socket of the client creating the channel.
 * @return {Channel *} newChannel - A pointer to the newly created channel, or NULL on failure.
 */
static Channel *createChannel(char *name, int maxSize, int clientSocket)
{
    lock_mutex();
    if (name == NULL || strlen(name) == 0)
    {
        fprintf(stderr, "Channel name cannot be empty\n");
        unlock_mutex();
        return NULL;
    }

    Channel *existingChannel = channelList.first;
    while (existingChannel != NULL)
    {
        if (strcmp(existingChannel->name, name) == 0)
        {
            fprintf(stderr, "Channel with this name already exists\n");
            unlock_mutex();
            return NULL;
        }
        existingChannel = existingChannel->next;
    }

    if (maxSize < -1)
    {
        fprintf(stderr, "Invalid max size for channel\n");
        unlock_mutex();
        return NULL;
    }

    Channel *newChannel = (Channel *)malloc(sizeof(Channel));
    if (newChannel == NULL)
    {
        perror("Failed to allocate memory for new channel");
        unlock_mutex();
        return NULL;
    }
    newChannel->name = strdup(name);
    newChannel->maxSize = maxSize;
    newChannel->clients = createList(clientSocket);
    newChannel->next = channelList.first->next;
    channelList.first->next = newChannel;
    unlock_mutex();
    return newChannel;
}

/**
 ** Get a channel by its name.
 * @param {char *} name - The name of the channel.
 * @return {Channel *} channel - A pointer to the channel, or NULL if not found.
 */
static Channel *getChannel(char *name)
{
    lock_mutex();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            unlock_mutex();
            return channel;
        }
        channel = channel->next;
    }
    unlock_mutex();
    return NULL;
}

/**
 ** Check if a channel is full.
 * @param {char *} name - The name of the channel.
 * @return {bool} - true if the channel is full, false otherwise.
 */
static bool isChannelFull(char *name)
{
    lock_mutex();
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        unlock_mutex();
        return false;
    }
    if (channel->maxSize == -1)
    {
        unlock_mutex();
        return false;
    }

    bool result = channel->clients->size > channel->maxSize;
    unlock_mutex();
    return result;
}

/**
 ** Get the size of a channel.
 * @param {char *} name - The name of the channel.
 * @return {int} - The size of the channel, or -1 if not found.
 */
static int getChannelSize(char *name)
{
    lock_mutex();
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        unlock_mutex();
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
    unlock_mutex();
    return size;
}

/**
 ** Check if a channel is empty.
 * @param {char *} name - The name of the channel.
 * @return {bool} - true if the channel is empty, false otherwise.
 */
static bool isChannelEmpty(char *name)
{
    lock_mutex();
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        unlock_mutex();
        return false;
    }
    if (isListEmpty(channel->clients))
    {
        fprintf(stderr, "Channel is empty\n");
        unlock_mutex();
        return true;
    }
    fprintf(stderr, "Channel is not empty\n");
    unlock_mutex();
    return false;
}

/**
 ** Get the name of the channel a client is in.
 * @param clientSocket The socket of the client.
 * @return {char *} The name of the channel, or NULL if not found.
 */
char *getClientChannelName(int clientSocket)
{
    lock_mutex();
    Channel *channel = getClientChannel(clientSocket);
    if (channel != NULL)
    {
        char *name = channel->name;
        unlock_mutex();
        return name;
    }
    unlock_mutex();
    return NULL;
}

/**
 ** Get the channel a client is in.
 * @param clientSocket The socket of the client.
 * @return {Channel *} A pointer to the channel, or NULL if not found.
 */
Channel *getClientChannel(int clientSocket)
{
    lock_mutex();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        Node *current = channel->clients->first;
        while (current != NULL)
        {
            if (current->val == clientSocket)
            {
                unlock_mutex();
                return channel;
            }
            current = current->next;
        }
        channel = channel->next;
    }
    unlock_mutex();
    return NULL;
}

/**
 ** Add a client to a specified channel name.
 * @param {char *} name - The name of the channel.
 * @param {int} clientSocket - The socket of the client.
 */
void addClientToChannel(char *name, int clientSocket)
{
    lock_mutex();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            if (isChannelFull(name))
            {
                fprintf(stderr, "Channel is full, cannot add client\n");
                unlock_mutex();
                return;
            }
            addLast(channel->clients, clientSocket);
            unlock_mutex();
            return;
        }
        channel = channel->next;
    }
    unlock_mutex();
}

/**
 ** Remove a client from a specified channel name.
 * @param {char *} name - The name of the channel.
 * @param {int} clientSocket - The socket of the client.
 */
void removeClientFromChannel(char *name, int clientSocket)
{
    lock_mutex();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            removeElement(channel->clients, clientSocket);
            if (isListEmpty(channel->clients) && strcmp(name, "Hub") != 0)
            {
                removeChannel(name);
            }
            unlock_mutex();
            return;
        }
        channel = channel->next;
    }
    unlock_mutex();
}

/**
 ** Remove a channel by its name.
 * @param {char *} name - The name of the channel.
 */
static void removeChannel(char *name)
{
    lock_mutex();
    if (name == NULL || strlen(name) == 0)
    {
        fprintf(stderr, "Channel name cannot be empty\n");
        unlock_mutex();
        return;
    }

    if (strcmp(channelList.first->name, name) == 0)
    {
        fprintf(stderr, "Cannot remove the hub channel\n");
        unlock_mutex();
        return;
    }

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
            unlock_mutex();
            return;
        }
        previous = current;
        current = current->next;
    }
    unlock_mutex();
}

/**
 ** Send a message to all clients in the channel of the specified client.
 * @param {int} clientSocket - The socket of the client.
 * @param {const char*} message - The message to send.
 */
void sendChannelMessage(int clientSocket, const char *message)
{
    lock_mutex();
    Channel *channel = getClientChannel(clientSocket);
    if (channel == NULL)
    {
        fprintf(stderr, "Client is not in any channel\n");
        unlock_mutex();
        return;
    }
    Node *current = channel->clients->first;
    while (current != NULL)
    {
        if (current->val != clientSocket)
        {
            send(current->val, message, strlen(message) + 1, 0);
        }
        current = current->next;
    }
    unlock_mutex();
}

/**
 ** Leave the channel of the specified client.
 * @param {int} clientSocket - The socket of the client.
 * @param {char*} response - The response message.
 * @param {size_t} responseSize - The size of the response buffer.
 * @return {bool} - true if successful, false otherwise.
 */
bool leaveChannel(int clientSocket, char *response, size_t responseSize)
{
    lock_mutex();
    Channel *clientChannel = getClientChannel(clientSocket);
    if (clientChannel == NULL)
    {
        snprintf(response, responseSize, "You are not in any channel");
        unlock_mutex();
        return false;
    }

    char *name = clientChannel->name;
    if (strcmp(name, "Hub") == 0)
    {
        snprintf(response, responseSize, "You cannot leave the Hub channel");
        unlock_mutex();
        return false;
    }

    unlock_mutex();
    sendFormattedChannelMessage(name, "Client%d has left the channel %s (%d/%d)",
                                clientSocket, name, getChannelSize(name) - 1, clientChannel->maxSize);

    sendFormattedChannelMessage("Hub", "Client%d has joined the channel %s (%d/%d)",
                                clientSocket, "Hub", getChannelSize("Hub"), -1);

    lock_mutex();
    removeClientFromChannel(name, clientSocket);
    addClientToChannel("Hub", clientSocket);
    unlock_mutex();
    return true;
}

/**
 ** Create and join a new channel.
 * @param {char*} name - The name of the channel.
 * @param {int} maxSize - The maximum size of the channel (-1 for unlimited).
 * @param {int} clientSocket - The socket of the client.
 * @param {char*} response - The response message.
 * @param {size_t} responseSize - The size of the response buffer.
 * @return {bool} - true if successful, false otherwise.
 */
bool createAndJoinChannel(char *name, int maxSize, int clientSocket, char *response, size_t responseSize)
{
    lock_mutex();
    if (name == NULL || strlen(name) == 0)
    {
        snprintf(response, responseSize, "Channel name cannot be empty");
        unlock_mutex();
        return false;
    }

    Channel *existingChannel = channelList.first;
    while (existingChannel != NULL)
    {
        if (strcmp(existingChannel->name, name) == 0)
        {
            snprintf(response, responseSize, "Channel with this name already exists");
            unlock_mutex();
            return false;
        }
        existingChannel = existingChannel->next;
    }

    if (maxSize < -1)
    {
        snprintf(response, responseSize, "Invalid max size for channel");
        unlock_mutex();
        return false;
    }

    unlock_mutex(); // Unlock before creating channel to avoid potential deadlocks
    Channel *newChannel = createChannel(name, maxSize, clientSocket);

    lock_mutex();
    char *currentChannel = getClientChannelName(clientSocket);
    unlock_mutex();

    if (currentChannel != NULL && strcmp(currentChannel, name) != 0)
    {
        sendFormattedChannelMessage(currentChannel, "Client%d has created a new channel named %s (%d/%d)",
                                    clientSocket, name, 1, maxSize);

        lock_mutex();
        removeClientFromChannel(currentChannel, clientSocket);
        unlock_mutex();
    }

    int currentSize = getChannelSize(name);
    snprintf(response, responseSize, "You have created and joined %s (%d/%d)", name, currentSize, maxSize);
    return true;
}

/**
 ** Join an existing channel.
 * @param {char*} name - The name of the channel.
 * @param {int} clientSocket - The socket of the client.
 * @param {char*} response - The response message.
 * @param {size_t} responseSize - The size of the response buffer.
 * @return {bool} - true if successful, false otherwise.
 */
bool joinChannel(char *name, int clientSocket, char *response, size_t responseSize)
{
    lock_mutex();
    if (name == NULL || strlen(name) == 0)
    {
        snprintf(response, responseSize, "Channel name cannot be empty");
        unlock_mutex();
        return false;
    }

    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        snprintf(response, responseSize, "Channel %s does not exist", name);
        unlock_mutex();
        return false;
    }

    if (isChannelFull(name))
    {
        snprintf(response, responseSize, "This channel is full, you cannot join it");
        unlock_mutex();
        return false;
    }

    Channel *clientChannel = getClientChannel(clientSocket);
    if (strcmp(clientChannel->name, name) == 0)
    {
        snprintf(response, responseSize, "You are already in this channel");
        unlock_mutex();
        return false;
    }

    Channel *currentChannel = getClientChannel(clientSocket);
    if (currentChannel != NULL)
    {
        removeClientFromChannel(currentChannel->name, clientSocket);
    }

    addClientToChannel(name, clientSocket);
    unlock_mutex();

    int currentSize = getChannelSize(name);
    snprintf(response, responseSize, "You have joined %s (%d/%d)", name, currentSize, channel->maxSize);

    sendFormattedChannelMessage(name, "Client%d has joined the channel %s (%d/%d)",
                                clientSocket, name, currentSize, channel->maxSize);

    sendFormattedChannelMessage(currentChannel->name, "Client%d has left the channel %s (%d/%d)",
                                clientSocket, currentChannel->name,
                                getChannelSize(currentChannel->name), currentChannel->maxSize);

    return true;
}

/**
 ** Send a message to all members of the channel of the specified client.
 * @param {int} clientSocket - The socket of the client.
 * @param {const char*} message - The message to send.
 */
void sendToAllChannelMembers(int clientSocket, const char *message)
{
    lock_mutex();
    Channel *channel = getClientChannel(clientSocket);
    if (channel == NULL)
    {
        fprintf(stderr, "Client is not in any channel\n");
        unlock_mutex();
        return;
    }

    Node *current = channel->clients->first;
    while (current != NULL)
    {
        send(current->val, message, strlen(message) + 1, 0);
        current = current->next;
    }
    unlock_mutex();
}

/**
 ** Send a message to all members of a specified channel, except the sender.
 * @param {char*} name - The name of the channel.
 * @param {const char*} message - The message to send.
 * @param {int} clientSocket - The socket of the sender.
 */
void sendToAllNamedChannelMembersExcept(char *name, const char *message, int clientSocket)
{
    lock_mutex();
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel %s does not exist\n", name);
        unlock_mutex();
        return;
    }

    Node *current = channel->clients->first;
    while (current != NULL)
    {
        if (current->val != clientSocket)
        {
            send(current->val, message, strlen(message) + 1, 0);
        }
        current = current->next;
    }
    unlock_mutex();
}

/**
 ** Send a message to all members of a specified channel.
 * @param {char*} name - The name of the channel.
 * @param {const char*} message - The message to send.
 */
void sendToAllNamedChannelMembers(char *name, const char *message)
{
    lock_mutex();
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel %s does not exist\n", name);
        unlock_mutex();
        return;
    }

    Node *current = channel->clients->first;
    while (current != NULL)
    {
        send(current->val, message, strlen(message) + 1, 0);
        current = current->next;
    }
    unlock_mutex();
}

/**
 ** Send a message to all clients in all channels.
 * @param {char*} message - The message to send.
 */
void sendInfoToAll(char *message)
{
    lock_mutex();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        Node *current = channel->clients->first;
        while (current != NULL)
        {
            send(current->val, message, strlen(message) + 1, 0);
            current = current->next;
        }
        channel = channel->next;
    }
    unlock_mutex();
}

/**
 ** Send a message to all clients in all channels, except the sender.
 * @param {int} clientSocket - The socket of the sender.
 * @param {char*} message - The message to send.
 */
void sendInfoToAllExcept(int clientSocket, char *message)
{
    lock_mutex();
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        Node *current = channel->clients->first;
        while (current != NULL)
        {
            if (current->val != clientSocket)
            {
                send(current->val, message, strlen(message) + 1, 0);
            }
            current = current->next;
        }
        channel = channel->next;
    }
    unlock_mutex();
}