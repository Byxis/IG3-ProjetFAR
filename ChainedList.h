#ifndef CHAINEDLIST_H
#define CHAINEDLIST_H

#include <pthread.h> // Add this include for pthread_mutex_t

// Forward declaration of User struct instead of including user.h
struct user;
typedef struct user User;

typedef struct node
{
    User *user;
    struct node *next;
} Node;

typedef struct List
{
    int size;
    Node *first;
    Node *curr;
    pthread_mutex_t mutex; // Add mutex to the list
} List;

List *createList(User *user);
int isListEmpty(List *c);
void addFirst(List *c, User *user);
void displayList(List *c);
void removeFirst(List *c);
void addLast(List *c, User *user);
void removeLast(List *c);
void removeElement(List *c, User *user);
void deleteList(List *c);

// Add thread-safe versions of list operations
void lockList(List *c);
void unlockList(List *c);
Node *findUserNodeSafe(List *c, User *user);

#endif // CHAINEDLIST_H
