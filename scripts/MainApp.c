#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define NUM_CLIENTES_DEFAULT 20
#define NUM_CAJEROS_DEFAULT 3

/*Libreria usada para facilitar el uso con booleanos
Se tiene en cuenta que relamente C lo guarda como 1(True) y 0 (False)*/
#include <stdbool.h>

/*Variables globales de los fichero*/
FILE *logFile;
const char *logFileName = "../logFiles/registroCaja.log";

/*Mutex para el control del acceso a recursos compartidos*/
// mutex creacion hilos
pthread_mutex_t mutexCreaHilos;
// Mutex cola clientes
pthread_mutex_t mutex_ColaClientes;

// Mutex logger
pthread_mutex_t mutex_Logger;

// hilo del reponedor
pthread_t hiloReponedor;

// condiciones y semáforos para controlar las llamadas del reponedor
pthread_mutex_t mutex_CajeroAviso;
pthread_cond_t condicion_ReponedorAviso;
int avisoCajero = 0; // Bandera para indicar si el reponedor debe avisar a un cajero
pthread_mutex_t mutex_ReponedorTerminado;
pthread_cond_t condicion_ReponedorTerminado;
int reponedorTerminado = 0;

int idClienteRep = 0;

// count numero de clientes
int numClientesTot;

// contador numero de clientes activos en el super
int numClientesActuales;

// Estructura cliente
struct clientes
{
    int idCliente;
    // 0:esperando, 1:en cajero, 2:atendido
    int estado;

    // hilo que ejecuta cada cliente
    pthread_t hiloCliente;
};

// Lista de clientes
struct clientes listaClientes[20];

/* Función que realiza las acciones de los clientes */
void *accionesClientes(void *IDCliente);

// cont numero de cajeros
int numCajeros;

/* Lista de 3 cajeros */
struct cajeros
{
    int idCajero;
    // 0:no, 1:sí
    int ocupado;
    // numero de clientes que lleva atentidos un cajero
    int clientesAtendidos;
    /* Hilo que ejecuta cada cajero */
    pthread_t hiloCajero;
};

// lista de cajeros
struct cajeros cajerosN[3];

// capacidad de la fila de clientes
int capacidadColaClientes;

// si el cajero esta cogiendo el descanso
// int cogeDescanso;

// funcion que realiza las operaciones del reponedor
void *accionesReponedor(void *arg);

/* Función que realiza las acciones de los cajeros */
void *accionesCajero(void *idCajero);

// funcion manejadora de nuevos clientes
void nuevoCliente(int sig);
// funcio manejadora de peticion de salida
void exitApp(int sig);

/*Funcion que escribe en el log*/
void writeLogMessage(char *id, char *msg);
int randomNumber(int min, int max);
int getPosCliente(int idClienteABuscar);

int esMenorId(int id);

int main(int argc, char *argv[])
{

    switch (argc)
    {
    // En caso de que pasemos dos argumentos, por ejemplo ./Pf 40
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

    srand(getpid());

    logFile = fopen(logFileName, "w");

    fclose(logFile);

    /* Realizamos la estructura sigaction y declaramos sus campos */
    struct sigaction sigacCliente;
    sigacCliente.sa_handler = nuevoCliente;
    sigacCliente.sa_flags = 0;
    /* Si recibimos por pantalla SIGUSR1 y SIGUSR2 llamaremos a la función nuevoCliente(); */
    if (-1 == sigaction(SIGUSR1, &sigacCliente, NULL))
    {
        perror("error sigaction nuevoCliente");
        exit(-1);
    }

    struct sigaction sigExit;
    sigExit.sa_handler = exitApp;
    sigExit.sa_flags = 0;
    // si recibimos la señal SIGINT se finaliza el programa
    if (-1 == sigaction(SIGINT, &sigExit, NULL))
    {
        perror("error en el sigaction de exit");
        exit(-1);
    }

    /* ----------------- Inicializamos los recursos ----------------- */

    /* Inicializamos los semáforos */
    if (pthread_mutex_init(&mutexCreaHilos, NULL) != 0)
        exit(-1);
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
    numClientesTot = 0;
    numClientesActuales = 0;

    /* Inicializamos la lista de clientes */

    for (int i = 0; i < capacidadColaClientes; i++)
    {
        /* Inicializamos los identificadores de los clientes del 1 al 20 */
        listaClientes[i].idCliente = 0;
        /* Inicializamos si el cliente está atendido o no, en este caso pondremos un 0 y el cliente no estará atendido, si fuera un 1 es que está siendo atendido */
        listaClientes[i].estado = 0;
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

    printf("\n\n----------------------------------------------- SUPERMECADO -----------------------------------------------\n\n");
    printf("Supermercado abierto con %d clientes máximos, %d cajeros.\n", capacidadColaClientes, numCajeros);
    printf("Introduzca 'kill -10 %d' desde otro terminal si desea introducir en el supermercado un nuevo cliente.\n", getpid());
    printf("Introduzca 'kill -2 %d' si desea finalizar el programa.\n", getpid());
    printf("*Revisa el fichero registroCaja.log para ver el registro de la ejecución.*\n");
    printf("\n------------------------------------------------------------------------------------------------------------\n\n");

    /* Guardamos en el log la apertura del Super */
    char aperturaLogSuperMercado[100];
    char superMercado[100];
    sprintf(superMercado, "Supermercado");
    sprintf(aperturaLogSuperMercado, "Apertura super");
    writeLogMessage(superMercado, aperturaLogSuperMercado);

    // creacion de hilos
    pthread_mutex_lock(&mutexCreaHilos);

    // Inicializamos los cajeros
    for (int i = 0; i < numCajeros; i++)
    {
        srand(time(NULL));
        if (pthread_create(&cajerosN[i].hiloCajero, NULL, accionesCajero, (void *)&cajerosN[i].idCajero) != 0)
        {
            exit(-1);
        }
    }

    // inicializamos al reponedor
    if (pthread_create(&hiloReponedor, NULL, accionesReponedor, NULL) != 0)
    {
        exit(-1);
    }

    pthread_mutex_unlock(&mutexCreaHilos);

    // Esperamos señales
    while (true)
    {
        pause();
    }

    return 0;
}

// funcion manejadora
void nuevoCliente(int sig)
{

    printf("Nuevo cliente quiere entrar al super, hay %d clientes actualmente en el super.\n", numClientesActuales + 1);
    /* Bloqueamos para evitar que entren 2 clientes a la vez */

    pthread_mutex_lock(&mutex_ColaClientes);
    if (numClientesActuales >= capacidadColaClientes)
    {

        printf("El cliente nuevo ha tenido que marchar, el supermercado estaba lleno.\n");
        pthread_mutex_unlock(&mutex_ColaClientes);
    }
    else
    {
        int i = 0;
        int posicionVacia = -1;
        // Buscamos una posición vacía en la lista de clientes
        while (i < capacidadColaClientes)
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
            numClientesTot++;

            /* nuevoCliente.id = contador clientes */
            listaClientes[posicionVacia].idCliente = numClientesTot;

            /* nuevoCliente.atendido = 0 */
            listaClientes[posicionVacia].estado = 0;

            /* Guardar en el log la hora de entrada del cliente en el supermercado */
            char entradaCliente[100];
            sprintf(entradaCliente, "Entra en el supermercado.");

            char nuevoCliente[100];
            sprintf(nuevoCliente, "cliente_%d ", listaClientes[posicionVacia].idCliente);
            writeLogMessage(nuevoCliente, entradaCliente);
            numClientesActuales++;
            /* Creamos el hilo para el cliente */
            pthread_create(&listaClientes[posicionVacia].hiloCliente, NULL, accionesClientes, (void *)&listaClientes[posicionVacia].idCliente);
            /* Desbloqueamos */
        }
        pthread_mutex_unlock(&mutex_ColaClientes);
    }
}

// funcion que lleva a cabo las acciones de los clientes
void *accionesClientes(void *idCliente)
{

    time_t startTime = time(NULL);

    /* Pasamos a entero el puntero *idCliente */

    int identificadorCliente = *(int *)idCliente;
    int posicion = getPosCliente(identificadorCliente);

    int num = 0;
    char nuevoCliente[100];
    sprintf(nuevoCliente, "cliente_%d ", identificadorCliente);
    while (1)
    {

        if (num == 0 && listaClientes[posicion].estado == 0)
        {

            time_t actualTime = time(NULL);
            if (actualTime - startTime > 10)
            {
                // caso1: en el 10% de los casos se simula que después de 10 segundos abandonan el super
                num = randomNumber(1, 100);

                if (num <= 10)
                {

                    /* Cliente no atendido en el tiempo de espera, abandona el supermercado */
                    char abandonaSupermercado[100];
                    sprintf(abandonaSupermercado, "Abandona el supermercado porque se ha cansado de esperar.");

                    writeLogMessage(nuevoCliente, abandonaSupermercado);

                    /* Borramos la información del cliente de la lista */

                    pthread_mutex_lock(&mutex_ColaClientes);
                    listaClientes[posicion].estado = 0;
                    listaClientes[posicion].idCliente = 0;

                    numClientesActuales--;
                    pthread_mutex_unlock(&mutex_ColaClientes);
                    /* Se da fin al hilo */
                    pthread_exit(NULL);
                }

                // caso2: sigue esperando
                else
                {
                }
            }
        }

        /* Comprobamos si el cliente fue atendido por un cajero */
        if (listaClientes[posicion].estado == 1)
        {

            /* Esperamos a que termine la atención del cajero */
            while (listaClientes[posicion].estado != 2)
            {
                // Esperamos
            }

            /* Borramos la información del cliente de la lista */
            pthread_mutex_lock(&mutex_ColaClientes);
            listaClientes[posicion].estado = 0;
            listaClientes[posicion].idCliente = 0;
            numClientesActuales--;
            pthread_mutex_unlock(&mutex_ColaClientes);

            /* Guardamos en el log la hora de finalización */
            char salidaCliente[100];
            sprintf(salidaCliente, "Sale del supermercado.");
            writeLogMessage(nuevoCliente, salidaCliente);
            pthread_exit(NULL);
        }
        else if (listaClientes[posicion].estado == 2)
        {

            /* Borramos la información del cliente de la lista */
            pthread_mutex_lock(&mutex_ColaClientes);
            listaClientes[posicion].estado = 0;
            listaClientes[posicion].idCliente = 0;
            numClientesActuales--;
            pthread_mutex_unlock(&mutex_ColaClientes);

            /* Guardamos en el log la hora de finalización */
            char salidaCliente[100];
            sprintf(salidaCliente, "Sale del supermercado.");
            writeLogMessage(nuevoCliente, salidaCliente);

            pthread_exit(NULL);
        }
    }
    /* Se da fin al hilo */
    pthread_exit(NULL);
}

// funcion que lleva a cabo las acciones de los cajeros
void *accionesCajero(void *idCajero)
{

    int i;
    // variable que representa el id del cliente a atender.
    int clienteAtendido;

    /* Pasamos a entero el puntero *idCajero */
    int identificadorCajero = *(int *)idCajero;
    char entradaCajero[100];
    char messageRep[100];
    char message[100];
    char mensajelog[100];
    char mensajeImporte[100];
    char cogerDescanso[100];
    char llegarDelDescanso[100];
    sprintf(entradaCajero, "Cajero_%d ", identificadorCajero + 1);

    char creacionCajeros[100];
    sprintf(creacionCajeros, "creado.", identificadorCajero);
    writeLogMessage(entradaCajero, creacionCajeros);


    /* Decimos que el cajero está libre */
    cajerosN[identificadorCajero].ocupado = 0;

    //  El cajero queda en un bucle infinito esperando a atender a un cliente y atendidiendole.

    while (1)
    {

        int indice;

        /* Bloqueamos la cola*/

        pthread_mutex_lock(&mutex_ColaClientes);
        /* Busco el primer cliente para atender, esto es el que más tiempo lleve esperando */
        /* Recorro una vez la lista para buscar un cliente que su ID sea diferente de 0 y que
           no haya sido atendido */
        for (int i = 0; i < capacidadColaClientes; i++)
        {

            identificadorCajero = *(int *)idCajero;
            sprintf(entradaCajero, "Cajero_%d ", identificadorCajero + 1);

            if (listaClientes[i].idCliente != 0 && listaClientes[i].estado == 0 && cajerosN[identificadorCajero].ocupado == 0)
            {

                cajerosN[identificadorCajero].ocupado = 1;
                cajerosN[identificadorCajero].clientesAtendidos++;

                clienteAtendido = listaClientes[i].idCliente;
                indice = i;
                /* Cambiamos el flag de atendido */
                listaClientes[i].estado = 1;
                pthread_mutex_unlock(&mutex_ColaClientes);

                // calculamos el tiempo que tarda en atender al cliente y esperamos ese tiempo
                int tiempoAtendido = randomNumber(1, 5);
                sleep(tiempoAtendido);

                if (listaClientes[i].idCliente != 0)
                {

                    sprintf(message, "Atiende al cliente_%d.", listaClientes[i].idCliente);
                    writeLogMessage(entradaCajero, message);

                    srand(identificadorCajero);
                    // calculamos un numero aleatorio entre 1 y 100, que representa qué ocurre en el cajero
                    int aleatorio = randomNumber(1, 100);

                    // primer caso: 25 % de que haya algún problema con el precio y haya que llamar al reponedor
                    if (aleatorio >= 71 && aleatorio <= 95)
                    {

                        sprintf(messageRep, "Ha habido un problema con el precio de la compra del cliente_%d y ha habido que llamar al reponedor.", listaClientes[i].idCliente);
                        writeLogMessage(entradaCajero, messageRep);

                        sprintf(mensajelog, "El cliente_%d ha realizado la compra correctamente después de haber llamado al reponedor.", listaClientes[i].idCliente);

                        // Lógica para avisar al reponedor
                        pthread_mutex_lock(&mutex_CajeroAviso);
                        avisoCajero = 1;
                        idClienteRep = listaClientes[i].idCliente;
                        pthread_cond_signal(&condicion_ReponedorAviso);
                        pthread_mutex_unlock(&mutex_CajeroAviso);

                        // Esperar a que el reponedor termine su trabajo
                        pthread_mutex_lock(&mutex_ReponedorTerminado);
                        while (reponedorTerminado == 0)
                        {
                            pthread_cond_wait(&condicion_ReponedorTerminado, &mutex_ReponedorTerminado);
                        }
                        reponedorTerminado = 0;
                        pthread_mutex_unlock(&mutex_ReponedorTerminado);

                        // segundo caso: 5% de que haya algún problema y no pueda realizar la compra (no tenga dinero, no funcione su tarjeta,etc)
                    }
                    else if (aleatorio >= 96 && aleatorio <= 100)
                    {

                        sprintf(mensajelog, "El cliente_%d  no puede realizar la compra debido a problemas con el pago.", listaClientes[i].idCliente);

                        // tercer caso: 70 % de que se pueda realizar la compra correctamente
                    }
                    else
                    {
                        sprintf(mensajelog, "El cliente_%d  realiza la compra sin ningún problema.", listaClientes[i].idCliente);
                    }

                    // escribimos el precio de la compra en el log
                    // calculamos el importe de su compra
                    int importe = randomNumber(1, 100);
                    sprintf(mensajeImporte, "Al cliente_%d le cuesta la compra un importe de %d.", listaClientes[i].idCliente, importe);
                    writeLogMessage(entradaCajero, mensajeImporte);

                    // escribimos qué ha pasado en el cajero
                    writeLogMessage(entradaCajero, mensajelog);
                }
                pthread_mutex_lock(&mutex_ColaClientes);
                // se cambia el flag de finalizado
                listaClientes[i].estado = 2;

                // se desbloquea el mutex de la cola de clientes
                pthread_mutex_unlock(&mutex_ColaClientes);

                // se comprueba que el cajero se tenga que coger el descanso

                if (cajerosN[identificadorCajero].clientesAtendidos % 10 == 0)
                {

                    /* Se registra el descanso */
                    sprintf(cogerDescanso, "Le toca coger descanso.");
                    writeLogMessage(entradaCajero, cogerDescanso);

                    /* Duerme 20 segundos */
                    sleep(20);

                    /* Se registra el descanso */
                    sprintf(llegarDelDescanso, "Vuelve de coger el descanso.");
                    writeLogMessage(entradaCajero, llegarDelDescanso);
                }
            }
        }

        // libera el mutex en caso de que no encuentre ningun cliente
        pthread_mutex_unlock(&mutex_ColaClientes);
        // se libera al cajero
        cajerosN[identificadorCajero].ocupado = 0;
    }
}

// funcion que lleva a cabo las acciones del reponedor
void *accionesReponedor(void *arg)
{
    char mensajelog[100];
    char idReponedor[100];
    sprintf(idReponedor, "Reponedor_1");

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
        int tiempoTrabajo = randomNumber(1, 5); // Supongamos que va de 1 a 6 segundos
        // Paso 3: Esperamos el tiempo
        sleep(tiempoTrabajo);
        // escribimos en el log
        sprintf(mensajelog, "El reponedor ha consultado el precio del producto del cliente_%d.", idClienteRep);
        writeLogMessage(idReponedor, mensajelog);

        // Paso 4: Avisamos de que ha terminado el reponedor
        pthread_mutex_lock(&mutex_ReponedorTerminado);
        reponedorTerminado = 1;
        pthread_cond_signal(&condicion_ReponedorTerminado);
        pthread_mutex_unlock(&mutex_ReponedorTerminado);
    }
    return NULL;
}

/*Funcion encargada de salir de la app*/
void exitApp(int sig)
{

    printf("Hasta pronto\n");
    char idCajero[100];
    char clientesAtendidosFrase[100];

    /* Guardamos en el log el total de clientes que ha atendido cada cajero */
    for (int i = 0; i < numCajeros; i++)
    {
        sprintf(idCajero, "Cajero_%d ", cajerosN[i].idCajero + 1);
        sprintf(clientesAtendidosFrase, "He atendido a %d clientes.", cajerosN[i].clientesAtendidos);
        writeLogMessage(idCajero, clientesAtendidosFrase);
    }

    /* Guardamos en el log el cierre del super */
    char superMercado[100];
    char cierreSuper[100];
    sprintf(superMercado, "Supermercado");
    sprintf(cierreSuper, "Cierre Super.");
    writeLogMessage(superMercado, cierreSuper);

    /* Finalizamos el programa */
    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
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

/**
 * Funcion que devuelve un numero aleatorio entre un minimo[n] y un maximo [m]
 */
int randomNumber(int n, int m)
{
    pthread_t tid = pthread_self();
    time_t seed = time(NULL) + (long)tid;
    srand((unsigned int)seed);
    return rand() % (m - n + 1) + n;
}

/*
 *Funcion que devuelve la posicion de un cliente en la lista de clientes
 */
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