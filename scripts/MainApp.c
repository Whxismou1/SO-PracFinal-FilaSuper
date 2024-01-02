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

#define NUM_CLIENTES_DEFAULT 20
#define NUM_CAJEROS_DEFAULT 3

/*Libreria usada para facilitar el uso con booleanos
Se tiene en cuenta que relamente C lo guarda como 1(True) y 0 (False)*/
#include <stdbool.h>

// /*Funcion usada para salir de la app en funcion del codigo de estado*/
// void exitApp(int status);

// // manejadora de los clientes
// void manejadoraClientes(int signal);

// void *AccionesCashier(void *arg);
// void *AccionesReponedor(void *arg);
// void *AccionesCliente(void *arg);

// void inicializeCashiers();
// void inicializeClientes();

// int getClientPosByID(int IDClient);

// void removeClient(int IDClient);

// /*Funcion encargada de devolver true o false de version numerica a string*/
// const char *getBoolean(bool value);

/*Variables globales de los fichero*/
FILE *logFile;
const char *logFileName = "../logFiles/registroCaja.log";

/*Mutex para el control del acceso a recursos compartidos*/
// Mutex cola clientes
pthread_mutex_t mutex_ColaClientes;
// Mutex logger
pthread_mutex_t mutex_Logger;

// count numero de clientes
int numClientes;

// Estructura cliente
struct clientes
{
    int idCliente;
    int estaSiendoAtendido;
    int finalizado;
    pthread_t hiloCliente;
};

// Lista de clientes
struct clientes listaClientes[20];

/* Función que realiza las acciones de los vehículos */
void *accionesClientes(void *IDCliente);

int numCajeros;

/* Lista de 3 cajeros */
struct cajeros
{
    int idCajero;
    int ocupado;
    int clientesAtendidos;
    /* Hilo que ejecuta cada cajero */
    pthread_t hiloCajero;
};

struct cajeros cajerosN[3];

int capacidadColaClientes;

/* Función que realiza las acciones de los cajeros */
void *accionesCajero(void *idCajero);
void nuevoCliente(int sig);
void exitApp();
/*Funcion que escribe en el log*/
void writeLogMessage(char *id, char *msg);
int randomNumber(int min, int max);
int getPosCliente(int idClienteABuscar);

int main(int argc, char *argv[])
{

    switch (argc)
    {
    // En caso de que pasemos dos argumentos, por ejemplo ./PracticaFinal 40
    case 2:
        capacidadColaClientes = atoi(argv[1]);
        numCajeros = NUM_CAJEROS_DEFAULT;
        if (capacidadColaClientes <= 0 || numCajeros <= 0)
        {
            numCajeros = NUM_CAJEROS_DEFAULT;
            capacidadColaClientes = NUM_CLIENTES_DEFAULT;
        }

        break;
    // En caso de que pasemos tres argumentos, por ejemplo ./Pf 30 6
    case 3:
        capacidadColaClientes = atoi(argv[1]);
        numCajeros = atoi(argv[2]);
        break;
    // Por defecto
    default:
        capacidadColaClientes = NUM_CLIENTES_DEFAULT;
        numCajeros = NUM_CAJEROS_DEFAULT;
        break;
    }

    /* Realizamos la estructura sigaction */
    struct sigaction e, s;
    e.sa_handler = nuevoCliente;
    s.sa_handler = exitApp;

    /* Si recibimos por pantalla SIGUSR1 y SIGUSR2 llamaremos a la función nuevoCliente(); */
    sigaction(SIGUSR1, &e, NULL);
    sigaction(SIGUSR2, &e, NULL);
    sigaction(SIGINT, &s, NULL);

    /* ----------------- Inicializamos los recursos ----------------- */

    /* Inicializamos los 2 semáforos */
    pthread_mutex_init(&mutex_ColaClientes, NULL);
    pthread_mutex_init(&mutex_Logger, NULL);

    /* Inicializamos el count de clientes */
    int numClientes = 0;

    /* Inicializamos la lista de clientes */

    for (int i = 0; i < capacidadColaClientes; i++)
    {
        /* Inicializamos los identificadores de los clientes del 1 al 10 */
        listaClientes[i].idCliente = 0;
        /* Inicializamos si el vehículo está atendido o no, en este caso pondremos un 0 y el vehículo no estará atendido, si fuera un 1 es que está siendo atendido */
        listaClientes[i].estaSiendoAtendido = 0;
        /* Inicializamos si ha finalizado la atención del vehículo, en este caso pondremos un 0 y el vehiculo no habrá finalizado */
        listaClientes[i].finalizado = 0;
    }

    /* Inicializamos la lista de cajeros */
    for (int i = 0; i < numCajeros; i++)
    {
        /* Inicializamos los identificadores de los cajeros del 0 al numero introducido */
        cajerosN[i].idCajero = i;
        /* Inicializamos si el cajero está ocupado o no, en este caso pondremos un 0 y el cajero no estará ocupado */
        cajerosN[i].ocupado = 0;
        /* Inicializamos el número de clientes que ha atendido cada cajero */
        cajerosN[i].clientesAtendidos = 0;
    }

    logFile = fopen(logFileName, "w");

    /* Guardamos en el log la apertura de Talleres Manolo */
    char aperturaLogSuperMercado[100];
    char superMercado[100];
    sprintf(superMercado, "Supermercado");
    sprintf(aperturaLogSuperMercado, "Apertura super");
    writeLogMessage(superMercado, aperturaLogSuperMercado);

    // Inicializamos los cajeros
    for (int i = 0; i < numCajeros; i++)
    {
        pthread_create(&cajerosN[i].hiloCajero, NULL, accionesCajero, (void *)&cajerosN[i].idCajero);
    }

    // Esperamos señal SIGUSR
    while (true)
    {
        pause();
    }
}

void writeLogMessage(char *id, char *msg)
{
    // Bloqueamos el mutex para que no se escriba en el log a la vez
    pthread_mutex_lock(&mutex_Logger);

    // Calculamos la hora actual
    time_t now = time(0);
    struct tm *tlocal = localtime(&now);
    char stnow[25];
    strftime(stnow, 25, " %d/ %m/ %y %H: %M: %S", tlocal);
    // Escribimos en el log
    logFile = fopen(logFileName, "a");
    fprintf(logFile, "[ %s] %s: %s\n", stnow, id, msg);
    fclose(logFile);
    pthread_mutex_unlock(&mutex_Logger);
}

void exitApp(int status)
{
    char idCajero[100];
    char clientesAtendidosFrase[100];

    /* Guardamos en el log el total de clientes que ha atendido cada cajero */
    for (int i = 0; i < numCajeros; i++)
    {
        sprintf(idCajero, "cajero_%d ", cajerosN[i].idCajero + 1);
        sprintf(clientesAtendidosFrase, "He atendido a %d clientes.", cajerosN[i].clientesAtendidos);
        writeLogMessage(idCajero, clientesAtendidosFrase);
    }

    /* Guardamos en el log el cierre de Talleres Manolo */
    char superMercado[100];
    char cierreSuper[100];
    sprintf(superMercado, "Supermercado");
    sprintf(cierreSuper, "Cierre Super.");
    writeLogMessage(superMercado, cierreSuper);

    /* Finalizamos el programa */
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

// const char *getBoolean(bool value)
// {
//     return value ? "true" : "false";
// }

// void manejadoraClientes(int signal)
// {
//     printf("Señal de creación de cliente recibida\n");
//     pid_t cliente = fork();
//     printf("Cliente con id %d creado\n", cliente);
// }

int randomNumber(int n, int m)
{
    // m max | n min
    srand(time(NULL));
    return rand() % (m - n + 1) + n;
}

int getPosCliente(int idClienteABuscar)
{
    for (int i = 0; i < capacidadColaClientes; i++)
    {
        if (listaClientes[i].idCliente == idClienteABuscar)
        {
            return i;
        }
    }
    return -1;
}

void *accionesCajero(void *idCajero)
{
    printf("Cajero %d creado\n", *(int *)idCajero + 1);
}

void nuevoCliente(int sig)
{
    printf("Señal de creación de cliente recibida\n");
}