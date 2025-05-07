#ifndef CHAINEDLIST_H
#define CHAINEDLIST_H

typedef struct node
{
    int val;
    struct node *next;
} Node;

typedef struct List
{
    int size;
    Node *first;
    Node *curr;
} List;

List *createList(int val);
int isListEmpty(List *c);
void addFirst(List *c, int val);
void displayList(List *c);
void removeFirst(List *c);
void addLast(List *c, int val);
void removeLast(List *c);
void removeElement(List *c, int val);

#endif // CHAINEDLIST_H
