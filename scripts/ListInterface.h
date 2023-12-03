#ifndef LIST_H
#define LIST_H

#include <stdbool.h>

/**
 * Queda ver que poner aqui, si cliente o cajero
 */
struct Cajero
{
    int id;
    bool status;
    int numClientesAtendidos;
    bool isResting;
};

struct Node
{
    struct Cajero *data;
    struct Node *next;
};

struct List
{
    struct Node *head;
};

struct List *createList();
void append(struct List *list, struct Cajero *cajero);
void printList(struct List *list);
const char *getBoolean(bool value);
#endif
