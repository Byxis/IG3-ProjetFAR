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

// Global channel list
extern ChannelList channelList;

// System initialization and cleanup
void initChannelSystem();
void cleanupChannelSystem();

// User channel operations
char *getUserChannelName(User *user);
Channel *getUserChannel(User *user);
void addUserToChannel(char *name, User *user);
void removeUserFromChannel(char *name, User *user);

// Channel management
bool userLeaveChannel(User *user, char *response, size_t responseSize);
bool userCreateAndJoinChannel(char *name, int maxSize, User *user, char *response, size_t responseSize);
bool userJoinChannel(char *name, User *user, char *response, size_t responseSize);

// Message sending
void sendUserChannelMessage(User *user, const char *message);
void sendToAllUserChannelMembers(User *user, const char *message);
void sendToAllNamedChannelMembersExceptUser(char *name, const char *message, User *user);
void sendToAllNamedChannelMembers(char *name, const char *message);
void sendInfoToAll(char *message);
void sendInfoToAllExceptUser(User *user, char *message);

// Helper functions
bool registerUserInHub(User *user);
void disconnectUserFromAllChannels(User *user);
User *findUserBySocketId(int socketId);
bool getUsersInChannel(char *channelName, char *result, size_t size);
bool listAllChannels(char *result, size_t size);

// Mutex helpers
void lockChannelList();
void unlockChannelList();

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
You have left channel1. You were the last one in this channel, it has been deleted.

*/