#ifndef CHANNEL_H
#define CHANNEL_H

#include "ChainedList.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct Channel Channel;

struct Channel
{
    char *name;    // name of the channel
    int maxSize;   // -1 for unlimited size
    List *clients; // list of clients in the channel
    Channel *next; // pointer to the next channel in the list
};

typedef struct ChannelList
{
    Channel *first; // first channel in the list
} ChannelList;

// System initialization and cleanup
void initChannelSystem();
void cleanupChannelSystem();

// Public API for channel operations
bool createAndJoinChannel(char *name, int maxSize, int clientSocket, char *response, size_t responseSize);
bool joinChannel(char *name, int clientSocket, char *response, size_t responseSize);
bool leaveChannel(int clientSocket, char *response, size_t responseSize);

// Client-related functions used by server
char *getClientChannelName(int clientSocket);
Channel *getClientChannel(int clientSocket);
void addClient(char *name, int clientSocket);
void removeClient(char *name, int clientSocket);

// Message sending functions
void sendChannelMessage(int clientSocket, const char *message);
void sendToAllChannelMembers(int clientSocket, const char *message);

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
