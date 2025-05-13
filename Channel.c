#include "Channel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <stdarg.h>

#define MESSAGE_BUFFER_SIZE 256
#define MESSAGE_BUFFER_COUNT 10

static char messageBuffers[MESSAGE_BUFFER_COUNT][MESSAGE_BUFFER_SIZE];
static int currentBufferIndex = 0;

static struct
{
    Channel *first;
} channelList = {NULL};

static Channel *getChannel(char *name);
static bool isChannelFull(char *name);
static int getChannelSize(char *name);
static bool isChannelEmpty(char *name);
static void *createChannel(char *name, int maxSize, int clientSocket);
static void removeChannel(char *name);

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
    Channel *channel = (Channel *)malloc(sizeof(Channel));
    if (channel == NULL)
    {
        perror("Failed to allocate memory for channel");
        exit(EXIT_FAILURE);
    }
    channel->name = strdup("Hub");
    channel->maxSize = -1;
    channel->clients = createList(-1);
    channel->next = NULL;
    channelList.first = channel;
}

/**
 ** Cleanup the channel system.
 * This function frees all memory associated with the channels and their clients.
 */
void cleanupChannelSystem()
{
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
    if (name == NULL || strlen(name) == 0)
    {
        fprintf(stderr, "Channel name cannot be empty\n");
        return NULL;
    }

    Channel *existingChannel = channelList.first;
    while (existingChannel != NULL)
    {
        if (strcmp(existingChannel->name, name) == 0)
        {
            fprintf(stderr, "Channel with this name already exists\n");
            return NULL;
        }
        existingChannel = existingChannel->next;
    }

    if (maxSize < -1)
    {
        fprintf(stderr, "Invalid max size for channel\n");
        return NULL;
    }

    Channel *newChannel = (Channel *)malloc(sizeof(Channel));
    if (newChannel == NULL)
    {
        perror("Failed to allocate memory for new channel");
        return NULL;
    }
    newChannel->name = strdup(name);
    newChannel->maxSize = maxSize;
    newChannel->clients = createList(clientSocket);
    newChannel->next = channelList.first->next;
    channelList.first->next = newChannel;
    return newChannel;
}

/**
 ** Get a channel by its name.
 * @param {char *} name - The name of the channel.
 * @return {Channel *} channel - A pointer to the channel, or NULL if not found.
 */
static Channel *getChannel(char *name)
{
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            return channel;
        }
        channel = channel->next;
    }
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

    return channel->clients->size > channel->maxSize;
}

/**
 ** Get the size of a channel.
 * @param {char *} name - The name of the channel.
 * @return {int} - The size of the channel, or -1 if not found.
 */
static int getChannelSize(char *name)
{
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        return -1;
    }
    if (strcmp(channel->name, "Hub") == 0)
    {
        return channel->clients->size - 1;
    }
    return channel->clients->size;
}

/**
 ** Check if a channel is empty.
 * @param {char *} name - The name of the channel.
 * @return {bool} - true if the channel is empty, false otherwise.
 */
static bool isChannelEmpty(char *name)
{
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel not found\n");
        return false;
    }
    if (isListEmpty(channel->clients))
    {
        fprintf(stderr, "Channel is empty\n");
        return true;
    }
    fprintf(stderr, "Channel is not empty\n");
    return false;
}

/**
 ** Get the name of the channel a client is in.
 * @param clientSocket The socket of the client.
 * @return {char *} The name of the channel, or NULL if not found.
 */
char *getClientChannelName(int clientSocket)
{
    Channel *channel = getClientChannel(clientSocket);
    if (channel != NULL)
    {
        return channel->name;
    }
    return NULL;
}

/**
 ** Get the channel a client is in.
 * @param clientSocket The socket of the client.
 * @return {Channel *} A pointer to the channel, or NULL if not found.
 */
Channel *getClientChannel(int clientSocket)
{
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        Node *current = channel->clients->first;
        while (current != NULL)
        {
            if (current->val == clientSocket)
            {
                return channel;
            }
            current = current->next;
        }
        channel = channel->next;
    }
    return NULL;
}

/**
 ** Add a client to a specified channel name.
 * @param {char *} name - The name of the channel.
 * @param {int} clientSocket - The socket of the client.
 */
void addClient(char *name, int clientSocket)
{
    Channel *channel = channelList.first;
    while (channel != NULL)
    {
        if (strcmp(channel->name, name) == 0)
        {
            if (isChannelFull(name))
            {
                fprintf(stderr, "Channel is full, cannot add client\n");
                return;
            }
            addLast(channel->clients, clientSocket);
            return;
        }
        channel = channel->next;
    }
}

/**
 ** Remove a client from a specified channel name.
 * @param {char *} name - The name of the channel.
 * @param {int} clientSocket - The socket of the client.
 */
void removeClient(char *name, int clientSocket)
{
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
            return;
        }
        channel = channel->next;
    }
}

/**
 ** Remove a channel by its name.
 * @param {char *} name - The name of the channel.
 */
static void removeChannel(char *name)
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
            return;
        }
        previous = current;
        current = current->next;
    }
}

/**
 ** Send a message to all clients in the channel of the specified client.
 * @param {int} clientSocket - The socket of the client.
 * @param {const char*} message - The message to send.
 */
void sendChannelMessage(int clientSocket, const char *message)
{
    Channel *channel = getClientChannel(clientSocket);
    if (channel == NULL)
    {
        fprintf(stderr, "Client is not in any channel\n");
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
    Channel *clientChannel = getClientChannel(clientSocket);
    if (clientChannel == NULL)
    {
        snprintf(response, responseSize, "You are not in any channel");
        return false;
    }

    char *name = clientChannel->name;
    if (strcmp(name, "Hub") == 0)
    {
        snprintf(response, responseSize, "You cannot leave the Hub channel");
        return false;
    }
    sendFormattedChannelMessage(name, "Client%d has left the channel %s (%d/%d)",
                                clientSocket, name, getChannelSize(name) - 1, clientChannel->maxSize);

    sendFormattedChannelMessage("Hub", "Client%d has joined the channel %s (%d/%d)",
                                clientSocket, "Hub", getChannelSize("Hub"), -1);

    removeClient(name, clientSocket);
    addClient("Hub", clientSocket);
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
    if (name == NULL || strlen(name) == 0)
    {
        snprintf(response, responseSize, "Channel name cannot be empty");
        return false;
    }

    Channel *existingChannel = channelList.first;
    while (existingChannel != NULL)
    {
        if (strcmp(existingChannel->name, name) == 0)
        {
            snprintf(response, responseSize, "Channel with this name already exists");
            return false;
        }
        existingChannel = existingChannel->next;
    }

    if (maxSize < -1)
    {
        snprintf(response, responseSize, "Invalid max size for channel");
        return false;
    }

    Channel *newChannel = createChannel(name, maxSize, clientSocket);

    char *currentChannel = getClientChannelName(clientSocket);
    if (currentChannel != NULL && strcmp(currentChannel, name) != 0)
    {
        sendFormattedChannelMessage(currentChannel, "Client%d has created a new channel named %s (%d/%d)",
                                    clientSocket, name, 1, maxSize);

        removeClient(currentChannel, clientSocket);
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

    if (strcmp(getClientChannel(clientSocket)->name, name) == 0)
    {
        snprintf(response, responseSize, "You are already in this channel");
        return false;
    }

    Channel *currentChannel = getClientChannel(clientSocket);
    if (currentChannel != NULL)
    {
        removeClient(currentChannel->name, clientSocket);
    }

    addClient(name, clientSocket);

    int currentSize = getChannelSize(name);
    snprintf(response, responseSize, "You have joined %s (%d/%d)", name, currentSize, channel->maxSize);

    // Utiliser les buffers pré-formatés
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
    Channel *channel = getClientChannel(clientSocket);
    if (channel == NULL)
    {
        fprintf(stderr, "Client is not in any channel\n");
        return;
    }

    Node *current = channel->clients->first;
    while (current != NULL)
    {
        send(current->val, message, strlen(message) + 1, 0);
        current = current->next;
    }
}

/**
 ** Send a message to all members of a specified channel, except the sender.
 * @param {char*} name - The name of the channel.
 * @param {const char*} message - The message to send.
 * @param {int} clientSocket - The socket of the sender.
 */
void sendToAllNamedChannelMembersExcept(char *name, const char *message, int clientSocket)
{
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel %s does not exist\n", name);
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
}

/**
 ** Send a message to all members of a specified channel.
 * @param {char*} name - The name of the channel.
 * @param {const char*} message - The message to send.
 */
void sendToAllNamedChannelMembers(char *name, const char *message)
{
    Channel *channel = getChannel(name);
    if (channel == NULL)
    {
        fprintf(stderr, "Channel %s does not exist\n", name);
        return;
    }

    Node *current = channel->clients->first;
    while (current != NULL)
    {
        send(current->val, message, strlen(message) + 1, 0);
        current = current->next;
    }
}

/**
 ** Send a message to all clients in all channels.
 * @param {char*} message - The message to send.
 */
void sendInfoToAll(char *message)
{
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
}

/**
 ** Send a message to all clients in all channels, except the sender.
 * @param {int} clientSocket - The socket of the sender.
 * @param {char*} message - The message to send.
 */
void sendInfoToAllExcept(int clientSocket, char *message)
{
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
}