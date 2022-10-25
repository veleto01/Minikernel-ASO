/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
        int id;				/* ident. del proceso */
        int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
        contexto_t contexto_regs;	/* copia de regs. de UCP */
        void * pila;			/* dir. inicial de la pila */
	BCPptr siguiente;		/* puntero a otro BCP */
	void *info_mem;			/* descriptor del mapa de memoria */

	unsigned int segundosDormir; //Segundos dormir
	
	int n_descriptores; //MUTEX -> Guarda el no. de descriptores abiertos del proceso
	
	int descriptores[NUM_MUT_PROC]; //MUTEX -> A�ADIDA. Array de descriptores
	
	//Round robin
	unsigned int slice; //Tiempo de ejecucion que le queda al proceso

} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};

/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;

//FUNCIONES AUXILIARES
void cuentaAtrasBloqueados();

int descriptor_libre();
int nombres_iguales(char* nombre);
int descriptor_mutex();


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int obtener_id_pr();
int dormir(unsigned int segundos);
int crear_mutex(char* nombre, int tipo);
int abrir_mutex(char* nombre);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);
int cerrar_mutex(unsigned int mutexid);
/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{obtener_id_pr},
					{dormir},
					{crear_mutex},
					{abrir_mutex},
					{lock},
					{unlock},
					{cerrar_mutex}};

// MUTEX
#define NO_RECURSIVO 0
#define RECURSIVO 1

//Estructura para el tipo mutex
typedef struct {
	char* nombre;
	int tipo; //Recursivo o no recursivo
	int propietario; //Id del proceso due�o, es decir, el que ha hecho lock
	int abierto; //Si no esta bierto = 0
	int locked; //N�mero de veces que ha sido bloqueado(0 si esta desbloqueado)
	int n_procesos_esperando; // numero de procesos esperando asociado a procesos_esperando

	//Guarda los procesos bloqueados de cada mutex bloqueado
	lista_BCPs lista_proc_esperando_lock;
} mutex;

//Array de mutex utilizados
mutex array_mutex[NUM_MUT];

//Numero de mutex creados
int mutex_creados;

//Lista de procesos bloqueados porque se habian creado el numero maximo de mutex permitidos
lista_BCPs lista_bloq_mutex = { NULL, NULL };

//ROUND ROBIN
int ticksPorRodaja;
BCPptr Proceso_Expulsar;

#endif /* _KERNEL_H */

