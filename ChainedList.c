#include "ChainedList.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "user.h" // Include this after ChainedList.h

List *createList(User *user)
{
    List *chain = (List *)malloc(sizeof(List));
    if (!chain)
        return NULL;

    // Set initial size based on whether user is NULL
    chain->size = user ? 1 : 0;

    // Initialize the mutex
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

// Add mutex locking functions
void lockList(List *c)
{
    if (c)
        pthread_mutex_lock(&c->mutex);
}

void unlockList(List *c)
{
    if (c)
        pthread_mutex_unlock(&c->mutex);
}

// Update remaining functions to use the encapsulated mutex when needed

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

int isListEmpty(List *c)
{
    return c == NULL || c->first == NULL;
}

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

    // Destroy the mutex
    pthread_mutex_destroy(&c->mutex);

    free(c);
}