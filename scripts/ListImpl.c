#include "ListInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct ListCajero *createListCajero()
{
    struct ListCajero *newList = (struct ListCajero *)malloc(sizeof(struct ListCajero));
    newList->head = NULL;
    return newList;
}

struct NodeCajeros *createNodeCajero(struct Cajero *cajero)
{
    struct NodeCajeros *newNode = (struct NodeCajeros *)malloc(sizeof(struct NodeCajeros));
    newNode->data = cajero;
    newNode->next = NULL;
    return newNode;
}

void appendCajero(struct ListCajero *list, struct Cajero *cajero)
{
    struct NodeCajeros *newNode = createNodeCajero(cajero);

    if (list->head == NULL)
    {
        list->head = newNode;
        return;
    }

    struct NodeCajeros *last = list->head;
    while (last->next != NULL)
    {
        last = last->next;
    }

    last->next = newNode;
}

void printListCajero(struct ListCajero *list)
{
    struct NodeCajeros *current = list->head;
    while (current != NULL)
    {
        printf("Id cajero: %d\n", current->data->cajeroID);
        printf("Status cajero: %s\n", getBoolean(current->data->status));
        printf("Num clientes atendidos: %d\n", current->data->numClientesAtendidos);
        printf("Is resting: %s\n", getBoolean(current->data->isResting));
        current = current->next;
    }
}

int getSizeListCajeros(struct ListCajero *list)
{
    int numElem = 0;

    struct NodeCajeros *aux = list->head;

    while (aux != NULL)
    {
        numElem++;
        aux = aux->next;
    }

    return numElem;
}