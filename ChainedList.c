#include "ChainedList.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

List *createList(int val)
{
    List *chain = (List *)malloc(sizeof(List));
    if (!chain)
        return NULL;

    chain->size = 1;
    chain->first = (Node *)malloc(sizeof(Node));
    if (!chain->first)
    {
        free(chain);
        return NULL;
    }

    chain->first->val = val;
    chain->first->next = NULL;
    chain->curr = chain->first;

    return chain;
}

void displayList(List *c)
{
    if (isListEmpty(c))
    {
        printf("[]\n");
        return;
    }

    printf("[");
    Node *nextNode = c->first;
    while (nextNode != NULL)
    {
        if (nextNode->next == NULL)
        {
            printf(" %d ]\n", nextNode->val);
        }
        else
        {
            printf(" %d,", nextNode->val);
        }
        nextNode = nextNode->next;
    }
}

int isListEmpty(List *c)
{
    return c == NULL || c->first == NULL;
}

void addFirst(List *c, int val)
{
    if (c == NULL)
        return;

    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
        return;

    newNode->val = val;
    newNode->next = c->first;

    c->first = newNode;
    c->size++;
}

void removeFirst(List *c)
{
    if (isListEmpty(c))
        return;

    Node *temp = c->first;
    c->first = c->first->next;

    if (c->curr == temp)
    {
        c->curr = c->first;
    }

    free(temp);
    c->size--;
}

void addLast(List *c, int val)
{
    if (c == NULL)
        return;

    if (isListEmpty(c))
    {
        c->first = (Node *)malloc(sizeof(Node));
        if (!c->first)
            return;

        c->first->val = val;
        c->first->next = NULL;
        c->curr = c->first;
        c->size = 1;
        return;
    }

    Node *lastNode = c->first;
    while (lastNode->next != NULL)
    {
        lastNode = lastNode->next;
    }

    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
        return;

    newNode->val = val;
    newNode->next = NULL;
    lastNode->next = newNode;
    c->size++;
}

void removeLast(List *c)
{
    if (isListEmpty(c))
        return;

    if (c->first->next == NULL)
    {
        free(c->first);
        c->first = NULL;
        c->curr = NULL;
        c->size = 0;
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
}

void removeElement(List *c, int val)
{
    if (isListEmpty(c))
        return;

    if (c->first->val == val)
    {
        removeFirst(c);
        return;
    }

    Node *current = c->first;
    while (current->next != NULL)
    {
        if (current->next->val == val)
        {
            Node *to_remove = current->next;

            if (c->curr == to_remove)
            {
                c->curr = current;
            }

            current->next = to_remove->next;
            free(to_remove);
            c->size--;
            return;
        }
        current = current->next;
    }
}