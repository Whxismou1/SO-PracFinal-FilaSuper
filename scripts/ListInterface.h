#ifndef LIST_H
#define LIST_H
#include <stdbool.h>

struct Cajero
{
    int cajeroID;
    bool status;
    int numClientesAtendidos;
    bool isResting;
};

struct NodeCajeros
{
    struct Cajero *data;
    struct NodeCajeros *next;
};

struct ListCajero
{
    struct NodeCajeros *head;
    int numClients;
};

struct Client
{
    int clientID;
    bool finishedShopping;

    bool isProcessed;
};

struct NodeClient
{
    struct Client *data;
    struct NodeClient *next;
};

struct ListClient
{
    struct NodeClient *head;
    int numClients;
};

struct ListCajero *createListCajero();
void appendCajero(struct ListCajero *list, struct Cajero *cajero);
void printListCajero(struct ListCajero *list);
const char *getBoolean(bool value);
int getSizeListCajeros(struct ListCajero *list);
#endif
