#ifndef CHANNEL_H
#define CHANNEL_H

#include "ChainedList.h"
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

typedef struct Channel
{
    char *name;
    int maxSize;
    List *clients;
    struct Channel *next;
} Channel;

typedef struct
{
    Channel *first;
    pthread_mutex_t mutex;
} ChannelList;

extern ChannelList channelList;

/**
 ** Initializes the channel system
 * @returns void
 */
void initChannelSystem();

/**
 ** Cleans up the channel system and frees all resources
 * @returns void
 */
void cleanupChannelSystem();

/**
 ** Gets the name of the channel a user is in
 * @param user (User*) - The user to check
 * @returns char* - The name of the channel or NULL if not found
 */
char *getUserChannelName(User *user);

/**
 ** Gets the channel a user is in
 * @param user (User*) - The user to check
 * @returns Channel* - Pointer to the channel or NULL if not found
 */
Channel *getUserChannel(User *user);

/**
 ** Adds a user to a channel
 * @param name (char*) - The name of the channel
 * @param user (User*) - The user to add
 * @returns void
 */
void addUserToChannel(char *name, User *user);

/**
 ** Removes a user from a channel
 * @param name (char*) - The name of the channel
 * @param user (User*) - The user to remove
 * @returns void
 */
void removeUserFromChannel(char *name, User *user);

/**
 ** Makes a user leave their current channel
 * @param user (User*) - The user leaving the channel
 * @param response (char*) - Buffer for response message
 * @param responseSize (size_t) - Size of response buffer
 * @returns bool - True if successful
 */
bool userLeaveChannel(User *user, char *response, size_t responseSize);

/**
 ** Creates a new channel and adds the user to it
 * @param name (char*) - The name for the new channel
 * @param maxSize (int) - Maximum number of users (-1 for unlimited)
 * @param user (User*) - The user creating the channel
 * @param response (char*) - Buffer for response message
 * @param responseSize (size_t) - Size of response buffer
 * @returns bool - True if successful
 */
bool userCreateAndJoinChannel(char *name, int maxSize, User *user, char *response, size_t responseSize);

/**
 ** Makes a user join an existing channel
 * @param name (char*) - The name of the channel to join
 * @param user (User*) - The user joining the channel
 * @param response (char*) - Buffer for response message
 * @param responseSize (size_t) - Size of response buffer
 * @returns bool - True if successful
 */
bool userJoinChannel(char *name, User *user, char *response, size_t responseSize);

/**
 ** Sends a message to all members of the user's channel
 * @param user (User*) - The user sending the message
 * @param message (const char*) - The message to send
 * @returns void
 */
void sendUserChannelMessage(User *user, const char *message);

/**
 ** Sends a message to all members of the user's channel
 * @param user (User*) - The user's channel to target
 * @param message (const char*) - The message to send
 * @returns void
 */
void sendToAllUserChannelMembers(User *user, const char *message);

/**
 ** Sends a message to all members of a channel except a specific user
 * @param name (char*) - The name of the channel
 * @param message (const char*) - The message to send
 * @param user (User*) - The user to exclude
 * @returns void
 */
void sendToAllNamedChannelMembersExceptUser(char *name, const char *message, User *user);

/**
 ** Sends a message to all members of a named channel
 * @param name (char*) - The name of the channel
 * @param message (const char*) - The message to send
 * @returns void
 */
void sendToAllNamedChannelMembers(char *name, const char *message);

/**
 ** Sends an informational message to all connected users
 * @param message (char*) - The message to send
 * @returns void
 */
void sendInfoToAll(char *message);

/**
 ** Sends an informational message to all users except one
 * @param user (User*) - The user to exclude
 * @param message (char*) - The message to send
 * @returns void
 */
void sendInfoToAllExceptUser(User *user, char *message);

/**
 ** Registers a user in the Hub channel
 * @param user (User*) - The user to register
 * @returns bool - True if successful
 */
bool registerUserInHub(User *user);

/**
 ** Disconnects a user from all channels
 * @param user (User*) - The user to disconnect
 * @returns void
 */
void disconnectUserFromAllChannels(User *user);

/**
 ** Finds a user by socket ID
 * @param socketId (int) - The socket ID to search for
 * @returns User* - The matching user or NULL if not found
 */
User *findUserBySocketId(int socketId);

/**
 ** Gets a list of users in a channel
 * @param channelName (char*) - The name of the channel
 * @param result (char*) - Buffer to store the result
 * @param size (size_t) - Size of the result buffer
 * @returns bool - True if successful
 */
bool getUsersInChannel(char *channelName, char *result, size_t size);

/**
 ** Lists all available channels
 * @param result (char*) - Buffer to store the result
 * @param size (size_t) - Size of the result buffer
 * @returns bool - True if successful
 */
bool listAllChannels(char *result, size_t size);

/**
 ** Locks the channel list mutex
 * @returns void
 */
void lockChannelList();

/**
 ** Unlocks the channel list mutex
 * @returns void
 */
void unlockChannelList();

/**
 ** Removes a channel by its name
 * @param name (char*) - The name of the channel to remove
 * @returns void
 */
void removeChannel(char *name);

/**
 ** Create a new channel, with the specified name and maximum size.
 * @param name (char*) - The name of the channel.
 * @param maxSize (int) - The maximum size of the channel (-1 for unlimited).
 * @param user (User*) - The user creating the channel (can be NULL).
 * @return Channel* - A pointer to the newly created channel, or NULL on failure.
 */
Channel *createChannel(char *name, int maxSize, User *user);
#endif // CHANNEL_H

/* Exemple usage:

- Commands :

@create <name> [maxSize]
You have created channel1

@join <name>
You have joined channel1
OR
This channel is full, you cannot join it

@leave <name>
You have left channel1
OR
You are not in this channel

- In situation :


    Client1:
Client2: Hey, come chat with me on channel1 !
Okay, I'm waiting for you
Client2 has created a new channel named channel1 (1/10)
@join channel1
You have joined channel1 (2/10)
Hey you !
@leave channel1
You have left channel1 (1/10)


    Client2:

Hey, come chat with me on channel1 !
Client1: Okay, I'm waiting for you
@create channel1 10
You have created and joined channel1 (1/10)
Client1 has joined the channel (2/10)
Client1: Hey you !
Client1 has left the channel (1/10)
Hello ! Oh, it's too late...
@leave channel1
You have left channel1.

*/