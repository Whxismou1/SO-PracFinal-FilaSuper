#include "ListInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct List* createList() {
    struct List* newList = (struct List*)malloc(sizeof(struct List));
    newList->head = NULL;
    return newList;
}

struct Node* createNode(struct Cajero* cajero) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    newNode->data = cajero;
    newNode->next = NULL;
    return newNode;
}

void append(struct List* list, struct Cajero* cajero) {
    struct Node* newNode = createNode(cajero);

    if (list->head == NULL) {
        list->head = newNode;
        return;
    }

    struct Node* last = list->head;
    while (last->next != NULL) {
        last = last->next;
    }

    last->next = newNode;
}

void printList(struct List* list) {
    struct Node* current = list->head;
    while (current != NULL) {
        printf("Id cajero: %d\n", current->data->id);
        printf("Status cajero: %s\n", getBoolean(current->data->status));
        printf("Num clientes atendidos: %d\n", current->data->numClientesAtendidos);
        printf("Is resting: %s\n", getBoolean(current->data->isResting));
        current = current->next;
    }
}
