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

/*Variables globales de los fichero*/
FILE *logFile;
const char *logFileName = "../logFiles/registroCaja.log";

/*Mutex para el control del acceso a recursos compartidos*/
// Mutex cola clientes
pthread_mutex_t mutex_ColaClientes;
// Mutex logger
pthread_mutex_t mutex_Logger;

pthread_t hiloReponedor;
pthread_mutex_t mutex_CajeroAviso = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condicion_ReponedorAviso = PTHREAD_COND_INITIALIZER;
int avisoCajero = 0; // Bandera para indicar si el reponedor debe avisar a un cajero
pthread_mutex_t mutex_ReponedorTerminado = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condicion_ReponedorTerminado = PTHREAD_COND_INITIALIZER;
int reponedorTerminado = 0;

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

void *accionesReponedor(void *arg);
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

    printf("\n\n----------------------------------------------- SUPERMECADO -----------------------------------------------\n\n");
    printf("Supermercado abierto con %d clientes, %d cajeros.\n", capacidadColaClientes, numCajeros);
    // printf("Si ha inicializado el programa con './PracticaFinal &' podrá simular la entrada de vehículos.\n");
    printf("Introduzca 'kill -10 PID' en el terminal si desea introducir en el taller un vehículo con avería de motor.\n");
    printf("Introduzca 'kill -2 PID' si desea finalizar el programa.\n");
    printf("Pulse intro para continuar...\n");

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
    if (pthread_mutex_init(&mutex_ColaClientes, NULL) != 0)
        exit(-1);
    if (pthread_mutex_init(&mutex_Logger, NULL) != 0)
        exit(-1);
    if (pthread_mutex_init(&mutex_ReponedorTerminado, NULL) != 0)
        exit(-1);
    if (pthread_mutex_init(&mutex_CajeroAviso, NULL) != 0)
        exit(-1);

    /* Inicializamos las variables condicion */
    if (pthread_cond_init(&condicion_ReponedorAviso, NULL) != 0)
        exit(-1);
    if (pthread_cond_init(&condicion_ReponedorTerminado, NULL) != 0)
        exit(-1);

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
        if (pthread_create(&cajerosN[i].hiloCajero, NULL, accionesCajero, (void *)&cajerosN[i].idCajero) != 0)
        {
            exit(-1);
        }
    }

    if (pthread_create(&hiloReponedor, NULL, accionesReponedor, NULL) != 0)
    {
        exit(-1);
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

    /* Pasamos a entero el puntero *idCajero */
    int identificadorCajero = *(int *)idCajero;

    char entradaCajero[100];
    sprintf(entradaCajero, "caj_%d ", identificadorCajero + 1);

    /* Decimos que el cajero está libre */
    cajerosN[identificadorCajero].ocupado = 0;

    int clienteAtendido, tiempoAtendido, importe;
    // El cajero queda en un bucle infinito esperando a atender a un cliente y atendidiendole.

    while (1)
    {
        int indice;
        /* Mientras el cajero este libre */
        while (cajerosN[identificadorCajero].ocupado == 0)
        {
            /* Bloqueamos la cola*/
            pthread_mutex_lock(&mutex_ColaClientes);

            /* Busco el primer cliente para atender, esto es el que más tiempo lleve esperando */
            /* Recorro una vez la lista para buscar un cliente que su ID sea diferente de 0 y que
               no haya sido atendido */
            for (int i = 0; i < capacidadColaClientes; i++)
            {
                if (listaClientes[i].idCliente != 0 && listaClientes[i].estaSiendoAtendido == 0 && listaClientes[i].finalizado == 0)
                {
                    clienteAtendido = listaClientes[i].idCliente;
                    indice = i;
                    /* Cambiamos el flag de atendido */
                    listaClientes[i].estaSiendoAtendido = 1;
                    /* Ponemos que el cajero está ocupado */
                    cajerosN[identificadorCajero].ocupado = 1;

                    tiempoAtendido = randomNumber(1, 5);
                    sleep(tiempoAtendido);
                    int aleatorio = randomNumber(1, 100);
                    if (aleatorio >= 71 && aleatorio <= 95)
                    {
                        // avisarReponedor();
                    }
                    else if (aleatorio >= 96 && aleatorio <= 100)
                    {
                        // no puede realizar la compra pq es Mou y se ha gastado todo en su pc :0
                    }

                    importe = randomNumber(1, 100);

                    pthread_mutex_unlock(&mutex_ColaClientes);
                }
            }

            pthread_mutex_unlock(&mutex_ColaClientes);

        } /* Fin del bucle infinito */

        /* Guardamos en el log la hora de atención */

        char IDclienteAtendido[200];
        sprintf(IDclienteAtendido, "Atiendo a cliente_%d.", clienteAtendido);
        writeLogMessage(entradaCajero, IDclienteAtendido);

        /* Llamamos a la función tipoAtencion */
        // tipoAtencion(IDclienteAtendido, identificadorCajero);

        /* Cambiamos el flag de atendido */
        pthread_mutex_lock(&mutex_ColaClientes);

        listaClientes[indice].finalizado = 1;
        pthread_mutex_unlock(&mutex_ColaClientes);

        /*  Incrementamos el contador de vehiculos atendidos */
        cajerosN[identificadorCajero].clientesAtendidos += 1;

        /* Mira si le toca descanso */
        if (cajerosN[identificadorCajero].clientesAtendidos % 10 == 0)
        {
            /* Se registra el descanso */
            char descanso[100];
            sprintf(descanso, "Le toca coger descanso.");
            writeLogMessage(entradaCajero, descanso);

            /* Duerme 20 segundos */
            sleep(20);

            /* Se registra el descanso */
            char salidaDescanso[100];
            sprintf(salidaDescanso, "Vuelve de coger el descanso.");
            writeLogMessage(entradaCajero, salidaDescanso);
        }
        /* El cajero quedaría libre */
        cajerosN[identificadorCajero].ocupado = 0;
    }
    /* Volvemos al paso 1 y buscamos el siguiente.*/
    /* Terminamos el hilo */
    pthread_exit(NULL);
}

void nuevoCliente(int sig)
{
    /* Bloqueamos para evitar que entren 2 vehículos a la vez */
    pthread_mutex_lock(&mutex_ColaClientes);

    int i = 0;
    int posicionVacia = -1;

    // Buscamos una posición vacía en la lista de clientes
    while (i < 10)
    {
        if (listaClientes[i].idCliente == 0)
        {
            posicionVacia = i;
            break;
        }
        i++;
    }

    // Si encontramos una posición vacía, creamos un nuevo cliente
    if (posicionVacia != -1)
    {
        /* Se añade el cliente */
        numClientes++;

        /* nuevoCliente.id = contador clientes */
        listaClientes[posicionVacia].idCliente = numClientes;

        /* nuevoCliente.atendido = 0 */
        listaClientes[posicionVacia].estaSiendoAtendido = 0;

        /* Creamos el hilo para el cliente */
        pthread_create(&listaClientes[posicionVacia].hiloCliente, NULL, accionesClientes, (void *)listaClientes[posicionVacia].idCliente);
    }
    // Si no hay espacio, ignoramos la llamada
    else
    {
        /* Ignoramos la llamada */
    }

    /* Desbloqueamos */
    pthread_mutex_unlock(&mutex_ColaClientes);
}

void *accionesClientes(void *idCliente)
{
    /* Pasamos a entero el puntero *idCliente */
    int identificadorCliente = *(int *)idCliente;
    int posicion = getPosCliente(identificadorCliente);

    /* Guardar en el log la hora de entrada del cliente en el supermercado */
    char entradaCliente[100];
    sprintf(entradaCliente, "Entra en el supermercado.");
    char nuevoCliente[100];
    sprintf(nuevoCliente, "cliente_%d ", identificadorCliente);
    writeLogMessage(nuevoCliente, entradaCliente);

    /* Dormir entre 1 y 5 segundos (tiempo de espera aleatorio) */
    int tiempoEspera = randomNumber(1, 5);
    sleep(tiempoEspera);

    /* Comprobamos si el cliente fue atendido por un cajero */
    if (listaClientes[posicion].estaSiendoAtendido == 1)
    {
        /* Esperamos a que termine la atención del cajero */
        while (listaClientes[posicion].finalizado == 0)
        {
            // Esperamos
        }

        /* Guardamos en el log la hora de finalización */
        char salidaCliente[100];
        sprintf(salidaCliente, "Sale del supermercado.");
        writeLogMessage(nuevoCliente, salidaCliente);

        /* Borramos la información del cliente de la lista */
        listaClientes[posicion].estaSiendoAtendido = 0;
        listaClientes[posicion].idCliente = 0;
        listaClientes[posicion].finalizado = 0;
    }
    else
    {
        /* Cliente no atendido en el tiempo de espera, abandona el supermercado */
        char abandonaSupermercado[100];
        sprintf(abandonaSupermercado, "Abandona el supermercado porque no ha sido atendido.");
        writeLogMessage(nuevoCliente, abandonaSupermercado);

        /* Borramos la información del cliente de la lista */
        listaClientes[posicion].estaSiendoAtendido = 0;
        listaClientes[posicion].idCliente = 0;
        listaClientes[posicion].finalizado = 0;
    }

    /* Se da fin al hilo */
    pthread_exit(NULL);
}

void *accionesReponedor(void *arg)
{
    while (true)
    {
        // Paso 1: Esperamos a que algún cajero nos avise
        pthread_mutex_lock(&mutex_CajeroAviso);
        while (!avisoCajero)
        {
            pthread_cond_wait(&condicion_ReponedorAviso, &mutex_CajeroAviso);
        }
        avisoCajero = 0;
        pthread_mutex_unlock(&mutex_CajeroAviso);

        // Paso 2: Calculamos el tiempo de trabajo (aleatorio)
        int tiempoTrabajo = rand() % 6 + 1; // Supongamos que va de 1 a 6 segundos

        // Paso 3: Esperamos el tiempo
        sleep(tiempoTrabajo);

        // Paso 4: Avisamos de que ha terminado el reponedor
        pthread_mutex_lock(&mutex_ReponedorTerminado);
        reponedorTerminado = 1;
        pthread_cond_signal(&condicion_ReponedorTerminado);
        pthread_mutex_unlock(&mutex_ReponedorTerminado);
    }
    return NULL;
}