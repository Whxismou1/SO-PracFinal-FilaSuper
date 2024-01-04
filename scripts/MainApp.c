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

//hilo del reponedor 
pthread_t hiloReponedor;

//condiciones y semáforos para controlar las llamadas del reponedor 
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
    //0:no, 1:sí 
    int estaSiendoAtendido;
    //0:no, 1:sí 
    int finalizado;

    //hilo que ejecuta cada cliente
    pthread_t hiloCliente;
};

// Lista de clientes
struct clientes listaClientes[20];

/* Función que realiza las acciones de los clientes */
void *accionesClientes(void *IDCliente);

//cont numero de cajeros
int numCajeros;

/* Lista de 3 cajeros */
struct cajeros
{
    int idCajero;
    //0:no, 1:sí 
    int ocupado;
    //numero de clientes que lleva atentidos un cajero
    int clientesAtendidos;
    /* Hilo que ejecuta cada cajero */
    pthread_t hiloCajero;
};

//lista de cajeros
struct cajeros cajerosN[3];

//capacidad de la fila de clientes
int capacidadColaClientes;

//funcion que realiza las operaciones del reponedor  
void *accionesReponedor(void *arg);

/* Función que realiza las acciones de los cajeros */
void *accionesCajero(void *idCajero);

//funcion manejadora de nuevos clientes
void nuevoCliente(int sig);
//funcio manejadora de peticion de salida
void exitApp(int sig);

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

    srand(getpid());
    logFile = fopen(logFileName, "w");

    fclose(logFile);

    /* Realizamos la estructura sigaction y declaramos sus campos */
    struct sigaction ss;
    ss.sa_handler = nuevoCliente;
    ss.sa_flags=0;
    /* Si recibimos por pantalla SIGUSR1 y SIGUSR2 llamaremos a la función nuevoCliente(); */
    if(-1==sigaction(SIGUSR1, &ss, NULL)){
        perror("error sigaction nuevoCliente");
        exit(-1);
    }

    struct sigaction s={0};
    s.sa_handler=exitApp;
    //si recibimos la señal SIGINT se finaliza el programa
    if(-1==sigaction(SIGINT, &s, NULL)){
        perror("error en el sigaction de exit");
        exit(-1);
    }
   

    printf("\n\n----------------------------------------------- SUPERMECADO -----------------------------------------------\n\n");
    printf("Supermercado abierto con %d clientes, %d cajeros.\n", capacidadColaClientes, numCajeros);
    // printf("Si ha inicializado el programa con './PracticaFinal &' podrá simular la entrada de vehículos.\n");
    printf("Introduzca 'kill -10 %d' desde otro terminal si desea introducir en el supermercado un nuevo cliente.\n", getpid());
    printf("Introduzca 'kill -2 %d' si desea finalizar el programa.\n", getpid());
    printf("Pulse intro para continuar...\n");



    /* ----------------- Inicializamos los recursos ----------------- */

    /* Inicializamos los semáforos */
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
        /* Inicializamos si el cliente está atendido o no, en este caso pondremos un 0 y el cliente no estará atendido, si fuera un 1 es que está siendo atendido */
        listaClientes[i].estaSiendoAtendido = 0;
        /* Inicializamos si ha finalizado la atención del cliente, en este caso pondremos un 0 y el cliente no habrá finalizado */
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


    /* Guardamos en el log la apertura del Super */
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

    //inicializamos al reponedor
    if (pthread_create(&hiloReponedor, NULL, accionesReponedor, NULL) != 0)
    {
        exit(-1);
    }

    // Esperamos señales
    while (1)
    {
        pause();
    }

    return 0;
}


//funcion manejadora 
void nuevoCliente(int sig){
    /* Bloqueamos para evitar que entren 2 clientes a la vez */

    printf("nuevo cliente\n");



    
    pthread_mutex_lock(&mutex_ColaClientes);

    

    int i = 0;
    int posicionVacia = -1;

    // Buscamos una posición vacía en la lista de clientes
    while (i < 20){
        if (listaClientes[i].idCliente == 0){
            posicionVacia = i;
            break;
        }
        i++;
    }

    // Si encontramos una posición vacía, creamos un nuevo cliente
    if (posicionVacia != -1){
        /* Se añade el cliente */
        numClientes++;

        /* nuevoCliente.id = contador clientes */
        listaClientes[posicionVacia].idCliente = numClientes;

        /* nuevoCliente.atendido = 0 */
        listaClientes[posicionVacia].estaSiendoAtendido = 0;
        printf("Cliente_%d creado", numClientes);
        /* Creamos el hilo para el cliente */
        pthread_create(&listaClientes[posicionVacia].hiloCliente, NULL, accionesClientes, (void *)&listaClientes[posicionVacia].idCliente);
    }
    // Si no hay espacio, ignoramos la llamada
    else
    {
        /* Ignoramos la llamada */

        printf("no hay hueco en el super para el nuevo cliente\n");
    }

    /* Desbloqueamos */
    pthread_mutex_unlock(&mutex_ColaClientes);
}

void exitApp(int sig){   

    printf("Hasta pronto\n");
    char idCajero[100];
    char clientesAtendidosFrase[100];

    /* Guardamos en el log el total de clientes que ha atendido cada cajero */
    for (int i = 0; i < numCajeros; i++)
    {
        sprintf(idCajero, "cajero_%d ", cajerosN[i].idCajero + 1);
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


//funcion que lleva a cabo las acciones de los cajeros
void *accionesCajero(void *idCajero){
    printf("Cajero %d creado\n", *(int *)idCajero + 1);

    int i;
    //variable que representa el id del cliente a atender.
    int clienteAtendido;
    

    /* Pasamos a entero el puntero *idCajero */
    int identificadorCajero = *(int *)idCajero;
    char entradaCajero[100];
    char message[100];
    char mensajelog[100];
    char mensajeImporte[100];
    char cogerDescanso[100];
    char llegarDelDescanso[100];
    sprintf(entradaCajero, "Cajero_%d ", identificadorCajero + 1);

    /* Decimos que el cajero está libre */
    cajerosN[identificadorCajero].ocupado = 0;

    //int tiempoAtendido, importe;
    // El cajero queda en un bucle infinito esperando a atender a un cliente y atendidiendole.

    while (1){
        //srand(time(NULL));
        int indice;
        /* Mientras el cajero este libre */
        while (cajerosN[identificadorCajero].ocupado == 0){
            /* Bloqueamos la cola*/
            pthread_mutex_lock(&mutex_ColaClientes);
            /* Busco el primer cliente para atender, esto es el que más tiempo lleve esperando */
            /* Recorro una vez la lista para buscar un cliente que su ID sea diferente de 0 y que
               no haya sido atendido */
            for (int i = 0; i < capacidadColaClientes; i++){
                if (listaClientes[i].idCliente != 0 && listaClientes[i].estaSiendoAtendido == 0 && listaClientes[i].finalizado == 0){
                    clienteAtendido = listaClientes[i].idCliente;
                    indice = i;
                    /* Cambiamos el flag de atendido */
                    listaClientes[i].estaSiendoAtendido = 1;
                    /* Ponemos que el cajero está ocupado */
                    cajerosN[identificadorCajero].ocupado = 1;
                     
                    //calculamos el tiempo que tarda en atender al cliente y esperamos ese tiempo
                    int tiempoAtendido = randomNumber(1, 5);
                    sleep(tiempoAtendido);

                    //calculamos un numero aleatorio entre 1 y 100, que representa qué ocurre en el cajero
                    int aleatorio = randomNumber(1, 100);

                    //primer caso: 25 % de que haya algún problema con el precio y haya que llamar al reponedor
                    if (aleatorio >= 71 && aleatorio <= 95){
                        
                        sprintf(mensajelog,"Ha habido un problema y ha habido que llamar al reponedor.");
                        // avisarReponedor();




                    //segundo caso: 5% de que haya algún problema y no pueda realizar la compra (no tenga dinero, no funcione su tarjeta,etc)
                    }else if (aleatorio >= 96 && aleatorio <= 100){
                        // no puede realizar la compra pq es Mou y se ha gastado todo en su pc :0
                        sprintf(mensajelog, "El cliente_%d  no puede realizar la compra debido a problemas con el pago.", listaClientes[i].idCliente);
                        printf("el cliente_%d no puede realizar la compra debido a problemas con el pago. ", listaClientes[i].idCliente);

                        
                    //tercer caso: 70 % de que se pueda realizar la compra correctamente
                    }else{
                        sprintf(mensajelog, "El cliente_%d  realiza la compra sin ningún problema.", listaClientes[i].idCliente);
                        printf("el cliente_%d realiza la compra sin ningún problema. ", listaClientes[i].idCliente);

                    
                    }

                    printf("El cliente_%d ha sido atendido correctamente.", listaClientes[i].idCliente);
                    sprintf(message, "El cliente_%d ha sido atendido correctamente.", listaClientes[i].idCliente);
                    writeLogMessage(entradaCajero, message);

                    //escribimos el precio de la compra en el log
                    //calculamos el importe de su compra
                    int importe = randomNumber(1, 100);
                    sprintf(mensajeImporte, "Al cliente_%d le ha costado la compra un importe de %d.", listaClientes[i].idCliente, importe);
                    writeLogMessage(entradaCajero, mensajeImporte);

                    //escribimos qué ha pasado en el cajero
                    sprintf(entradaCajero, mensajelog);

                    //se cambia el flag de finalizado
                    listaClientes[i].finalizado=1;

                    //se desbloquea el mutex de la cola de clientes
                    pthread_mutex_unlock(&mutex_ColaClientes);

                    //se suma uno al numero de clientes atendidos por el cajero
                    cajerosN[identificadorCajero].clientesAtendidos+=1;

                    //se comprueba que el cajero se tenga que coger el descanso
                    
                    if (cajerosN[identificadorCajero].clientesAtendidos % 10 == 0){
                        /* Se registra el descanso */
                        sprintf(cogerDescanso, "Le toca coger descanso.");
                        writeLogMessage(entradaCajero, cogerDescanso);

                        /* Duerme 20 segundos */
                        sleep(20);

                        /* Se registra el descanso */
                        sprintf(llegarDelDescanso, "Vuelve de coger el descanso.");
                        writeLogMessage(entradaCajero, llegarDelDescanso);
                    }

                    //libera el mutex
                     pthread_mutex_unlock(&mutex_ColaClientes);
                     
                     //se libera al cajero
                     cajerosN[identificadorCajero].ocupado = 0;
                }
            }
        } 
    }
}




//funcion que lleva a cabo las acciones de los clientes
void *accionesClientes(void *idCliente){

    /* Pasamos a entero el puntero *idCliente */
    int identificadorCliente = *(int *)idCliente;
    int posicion = getPosCliente(identificadorCliente);

    /* Guardar en el log la hora de entrada del cliente en el supermercado */
    char entradaCliente[100];
    sprintf(entradaCliente, "Entra en el supermercado.");
    printf("el cliente entra en el supermercado");
    char nuevoCliente[100];
    sprintf(nuevoCliente, "cliente_%d ", identificadorCliente);
    writeLogMessage(nuevoCliente, entradaCliente);

    /* Dormir entre 1 y 5 segundos (tiempo de espera aleatorio) */
    int tiempoEspera = randomNumber(1, 5);
    sleep(tiempoEspera);

    /* Comprobamos si el cliente fue atendido por un cajero */
    if (listaClientes[posicion].estaSiendoAtendido == 1){

        /* Esperamos a que termine la atención del cajero */
        while (listaClientes[posicion].finalizado == 0){

            // Esperamos
            
        }

        /* Guardamos en el log la hora de finalización */
        char salidaCliente[100];
        sprintf(salidaCliente, "Sale del supermercado.");
        printf("el cliente sale del super");
        writeLogMessage(nuevoCliente, salidaCliente);

        /* Borramos la información del cliente de la lista */
        listaClientes[posicion].estaSiendoAtendido = 0;
        listaClientes[posicion].idCliente = 0;
        listaClientes[posicion].finalizado = 0;
    }else{

        /* Cliente no atendido en el tiempo de espera, abandona el supermercado */
        char abandonaSupermercado[100];
        sprintf(abandonaSupermercado, "Abandona el supermercado porque no ha sido atendido.");
        printf("el cliente abandona el supermercado");
        writeLogMessage(nuevoCliente, abandonaSupermercado);

        /* Borramos la información del cliente de la lista */
        listaClientes[posicion].estaSiendoAtendido = 0;
        listaClientes[posicion].idCliente = 0;
        listaClientes[posicion].finalizado = 0;
    }

    /* Se da fin al hilo */
    pthread_exit(NULL);
}


//funcion que lleva a cabo las acciones del reponedor 
void *accionesReponedor(void *arg){
    char mensajelog[100];
    char idReponedor[100];
    sprintf(idReponedor, "Reponedor_1");

    while (true){
        // Paso 1: Esperamos a que algún cajero nos avise
        pthread_mutex_lock(&mutex_CajeroAviso);

        while (!avisoCajero){
            pthread_cond_wait(&condicion_ReponedorAviso, &mutex_CajeroAviso);
        }
        avisoCajero = 0;
        pthread_mutex_unlock(&mutex_CajeroAviso);

        // Paso 2: Calculamos el tiempo de trabajo (aleatorio)
        int tiempoTrabajo = randomNumber(1,5); // Supongamos que va de 1 a 6 segundos
        //escribimos en el log
        sprintf(mensajelog, "El reponedor ha consultado el precio del producto.");
        writeLogMessage(idReponedor, mensajelog);
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