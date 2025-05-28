#ifndef CHAINEDLIST_H
#define CHAINEDLIST_H

#include <pthread.h>

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
    pthread_mutex_t mutex;
} List;

/**
 ** Creates a new list with an optional initial user
 * @param user (User*) - The user to add to the list (can be NULL)
 * @returns List* - Pointer to the new list
 */
List *createList(User *user);

/**
 ** Checks if a list is empty
 * @param c (List*) - The list to check
 * @returns int - 1 if empty, 0 otherwise
 */
int isListEmpty(List *c);

/**
 ** Adds a user to the beginning of the list
 * @param c (List*) - The list to modify
 * @param user (User*) - The user to add
 * @returns void
 */
void addFirst(List *c, User *user);

/**
 ** Displays the contents of the list
 * @param c (List*) - The list to display
 * @returns void
 */
void displayList(List *c);

/**
 ** Removes the first element from the list
 * @param c (List*) - The list to modify
 * @returns void
 */
void removeFirst(List *c);

/**
 ** Adds a user to the end of the list
 * @param c (List*) - The list to modify
 * @param user (User*) - The user to add
 * @returns void
 */
void addLast(List *c, User *user);

/**
 ** Removes the last element from the list
 * @param c (List*) - The list to modify
 * @returns void
 */
void removeLast(List *c);

/**
 ** Removes a specific user from the list
 * @param c (List*) - The list to modify
 * @param user (User*) - The user to remove
 * @returns void
 */
void removeElement(List *c, User *user);

/**
 ** Deletes the list and frees all memory
 * @param c (List*) - The list to delete
 * @returns void
 */
void deleteList(List *c);

/**
 ** Locks the list mutex for thread-safe operations
 * @param c (List*) - The list to lock
 * @returns void
 */
void lockList(List *c);

/**
 ** Unlocks the list mutex after thread-safe operations
 * @param c (List*) - The list to unlock
 * @returns void
 */
void unlockList(List *c);

/**
 ** Finds a node containing a specific user in a thread-safe manner
 * @param c (List*) - The list to search
 * @param user (User*) - The user to find
 * @returns Node* - The node containing the user or NULL if not found
 */
Node *findUserNodeSafe(List *c, User *user);

#endif // CHAINEDLIST_H
