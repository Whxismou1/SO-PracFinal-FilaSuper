#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include "ListInterface.h"
#define NUMCLIENTSDEFAULT 20
#define NUMCASHIERSDEFAULT 3

/*Libreria usada para facilitar el uso con booleanos
Se tiene en cuenta que relamente C lo guarda como 1(True) y 0 (False)*/
#include <stdbool.h>

/*Funcion que escribe en el log*/
void writeLogMessage(char *id, char *msg);

/*Funcion usada para salir de la app en funcion del codigo de estado*/
void exitApp(int status);

/*Funcion encargada de devolver true o false de version numerica a string*/
const char *getBoolean(bool value);

/*Variables globales de los fichero*/
FILE *logFile;
const char *logFileName = "../logFiles/registroCaja.log";
pthread_mutex_t mutex_LineCustomers;
pthread_mutex_t mutex_Logger;
pthread_mutex_t mutex_CustomersOnLine;

int numCustomers, numCashiers;

int main(int argc, char *argv[])
{

    if (argc == 3)
    {
        numCustomers = atoi(argv[1]);
        numCashiers = atoi(argv[2]);

        if (numCustomers <= 0 || numCashiers <= 0)
        {
            numCashiers = NUMCASHIERSDEFAULT;
            numCustomers = NUMCLIENTSDEFAULT;
        }
    }
    else
    {
        numCustomers = NUMCLIENTSDEFAULT;
        numCashiers = NUMCASHIERSDEFAULT;
    }

    // /*Si existe el fichero se elimina*/
    // remove(logFileName);

    // /*Ejemplo basico de uso de la struct*/
    // struct Cajero cajero1 = {1, true, 0, false};

    // printf("Id cajero: %d\n", cajero1.cajeroID);
    // printf("Status cajero: %s\n", getBoolean(cajero1.status));
    // printf("Num clientes atendidos: %d\n", cajero1.numClientesAtendidos);
    // printf("Is resting: %s\n", getBoolean(cajero1.isResting));

    // writeLogMessage("ID123", "Este es un mensaje de prueba.");

    /**
     * Uso basico de la lista
     */

    // Creacion de la lista
    // struct ListCajero *cajeroLista = createListCajero();

    // struct Cajero cajero2 = {2, false, 3, true};

    // // Adicion de los elementos a la lista
    // appendCajero(cajeroLista, &cajero1);
    // appendCajero(cajeroLista, &cajero2);
    // appendCajero(cajeroLista, &cajero2);

    // // impresion lista actual
    // printf("Lista actual cajeros:\n");
    // printListCajero(cajeroLista);

    // return 0;
}

void writeLogMessage(char *id, char *msg)
{
    // Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, " %d/ %m/ %y %H: %M: %S", tlocal);
    // Escribimos en el log
    logFile = fopen(logFileName, "a");
    fprintf(logFile, "[ %s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
}

void exitApp(int status)
{
    exit(status);
}

const char *getBoolean(bool value)
{
    return value ? "true" : "false";
}