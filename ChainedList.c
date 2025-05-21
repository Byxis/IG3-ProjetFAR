#include "ChainedList.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "user.h"

/**
 ** Creates a new list with an optional initial user
 * @param user (User*) - The user to add to the list (can be NULL)
 * @returns List* - Pointer to the new list
 */
List *createList(User *user)
{
    List *chain = (List *)malloc(sizeof(List));
    if (!chain)
        return NULL;

    chain->size = user ? 1 : 0;
    pthread_mutex_init(&chain->mutex, NULL);

    if (user)
    {
        chain->first = (Node *)malloc(sizeof(Node));
        if (!chain->first)
        {
            free(chain);
            return NULL;
        }
        chain->first->user = user;
        chain->first->next = NULL;
        chain->curr = chain->first;
    }
    else
    {
        chain->first = NULL;
        chain->curr = NULL;
    }

    return chain;
}

/**
 ** Locks the list mutex for thread-safe operations
 * @param c (List*) - The list to lock
 * @returns void
 */
void lockList(List *c)
{
    if (c)
        pthread_mutex_lock(&c->mutex);
}

/**
 ** Unlocks the list mutex after thread-safe operations
 * @param c (List*) - The list to unlock
 * @returns void
 */
void unlockList(List *c)
{
    if (c)
        pthread_mutex_unlock(&c->mutex);
}

/**
 ** Displays the contents of the list
 * @param c (List*) - The list to display
 * @returns void
 */
void displayList(List *c)
{
    lockList(c);

    if (isListEmpty(c))
    {
        printf("[]\n");
        unlockList(c);
        return;
    }

    printf("[");
    Node *nextNode = c->first;
    while (nextNode != NULL)
    {
        if (nextNode->next == NULL)
        {
            printf(" %s ]\n", nextNode->user->name);
        }
        else
        {
            printf(" %s,", nextNode->user->name);
        }
        nextNode = nextNode->next;
    }

    unlockList(c);
}

/**
 ** Checks if a list is empty
 * @param c (List*) - The list to check
 * @returns int - 1 if empty, 0 otherwise
 */
int isListEmpty(List *c)
{
    return c == NULL || c->first == NULL;
}

/**
 ** Adds a user to the beginning of the list
 * @param c (List*) - The list to modify
 * @param user (User*) - The user to add
 * @returns void
 */
void addFirst(List *c, User *user)
{
    if (c == NULL)
        return;

    lockList(c);

    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        unlockList(c);
        return;
    }

    newNode->user = user;
    newNode->next = c->first;

    c->first = newNode;
    c->size++;

    unlockList(c);
}

/**
 ** Removes the first element from the list
 * @param c (List*) - The list to modify
 * @returns void
 */
void removeFirst(List *c)
{
    if (isListEmpty(c))
        return;

    lockList(c);

    Node *temp = c->first;
    c->first = c->first->next;

    if (c->curr == temp)
    {
        c->curr = c->first;
    }

    free(temp);
    c->size--;

    unlockList(c);
}

/**
 ** Adds a user to the end of the list
 * @param c (List*) - The list to modify
 * @param user (User*) - The user to add
 * @returns void
 */
void addLast(List *c, User *user)
{
    if (c == NULL)
        return;

    if (user == NULL)
        return;

    lockList(c);

    if (isListEmpty(c))
    {
        c->first = (Node *)malloc(sizeof(Node));
        if (!c->first)
        {
            unlockList(c);
            return;
        }

        c->first->user = user;
        c->first->next = NULL;
        c->curr = c->first;
        c->size = 1;
        unlockList(c);
        return;
    }

    Node *lastNode = c->first;
    while (lastNode->next != NULL)
    {
        lastNode = lastNode->next;
    }

    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        unlockList(c);
        return;
    }

    newNode->user = user;
    newNode->next = NULL;
    lastNode->next = newNode;
    c->size++;

    unlockList(c);
}

/**
 ** Removes the last element from the list
 * @param c (List*) - The list to modify
 * @returns void
 */
void removeLast(List *c)
{
    if (isListEmpty(c))
        return;

    lockList(c);

    if (c->first->next == NULL)
    {
        free(c->first);
        c->first = NULL;
        c->curr = NULL;
        c->size = 0;
        unlockList(c);
        return;
    }

    Node *prevToLast = c->first;
    while (prevToLast->next->next != NULL)
    {
        prevToLast = prevToLast->next;
    }

    if (c->curr == prevToLast->next)
    {
        c->curr = prevToLast;
    }

    free(prevToLast->next);
    prevToLast->next = NULL;
    c->size--;

    unlockList(c);
}

/**
 ** Finds a node containing a specific user in a thread-safe manner
 * @param c (List*) - The list to search
 * @param user (User*) - The user to find
 * @returns Node* - The node containing the user or NULL if not found
 */
Node *findUserNodeSafe(List *c, User *user)
{
    if (c == NULL || user == NULL)
        return NULL;

    lockList(c);

    Node *current = c->first;
    while (current != NULL)
    {
        if (current->user == user)
        {
            unlockList(c);
            return current;
        }
        current = current->next;
    }

    unlockList(c);
    return NULL;
}

/**
 ** Removes a specific user from the list
 * @param c (List*) - The list to modify
 * @param user (User*) - The user to remove
 * @returns void
 */
void removeElement(List *c, User *user)
{
    if (isListEmpty(c))
        return;

    lockList(c);

    if (c->first->user == user)
    {
        Node *to_remove = c->first;
        c->first = c->first->next;

        if (c->curr == to_remove)
        {
            c->curr = c->first;
        }

        free(to_remove);
        c->size--;
        unlockList(c);
        return;
    }

    Node *current = c->first;
    while (current->next != NULL)
    {
        if (current->next->user == user)
        {
            Node *to_remove = current->next;

            if (c->curr == to_remove)
            {
                c->curr = current;
            }

            current->next = to_remove->next;
            free(to_remove);
            c->size--;
            unlockList(c);
            return;
        }
        current = current->next;
    }

    unlockList(c);
}

/**
 ** Deletes the list and frees all memory
 * @param c (List*) - The list to delete
 * @returns void
 */
void deleteList(List *c)
{
    if (c == NULL)
        return;

    lockList(c);

    Node *current = c->first;
    while (current != NULL)
    {
        Node *temp = current;
        current = current->next;
        free(temp);
    }

    unlockList(c);

    pthread_mutex_destroy(&c->mutex);

    free(c);
}