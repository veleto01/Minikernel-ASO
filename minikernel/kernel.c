/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */
#include <string.h>

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Funci�n que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Funci�n que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	//printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al m�nimo el nivel de interrupci�n mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Funci�n de planificacion que implementa un algoritmo FIFO.
 */
static BCP *planificador()
{
	//Como tenemos que implementar el RR tendremos que asignar 
	//El valosr correspondiente a "ticksPorRodaja" y a "Proceso_Expulsar" reiniciarlo a 0
	ticksPorRodaja = TICKS_POR_RODAJA;
	Proceso_Expulsar = NULL;
	while (lista_listos.primero == NULL)
		espera_int(); /* No hay nada que hacer */
	return lista_listos.primero;
}

static void roundRobin()
{
	ticksPorRodaja--;
	if (ticksPorRodaja <= 0)
	{
		Proceso_Expulsar = p_proc_actual;
		activar_int_SW();
	}
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no deber�a llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no deber�a llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	//printk("-> TRATANDO INT. DE RELOJ\n");
	
	
	//////////
	//Dormir//
	//////////
	
	cuentaAtrasBloqueados();
	
	
	///////////////
	//Round Robin//
	///////////////
	
	roundRobin();

        return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw()
{
	int interrupcion; 
	interrupcion=fijar_nivel_int(NIVEL_1);
	if (p_proc_actual == Proceso_Expulsar)
	{
		BCPptr inmediato = p_proc_actual;
		interrupcion = fijar_nivel_int(NIVEL_3);
		eliminar_primero(&lista_listos);
		insertar_ultimo(&lista_listos, inmediato);
		p_proc_actual = planificador();
		fijar_nivel_int(interrupcion);
		//Llamamos al "Cambiador de contexto" para salvaguardar el contexto que se esta guardando junto
		//con el que se esta restaurando.
		cambio_contexto(&(inmediato->contexto_regs),&(p_proc_actual->contexto_regs));
	}
	fijar_nivel_int(interrupcion);

	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

proc = buscar_BCP_libre();
if (proc == -1)
return -1;	/* no hay entrada libre */

/* A rellenar el BCP ... */
p_proc = &(tabla_procs[proc]);

/* crea la imagen de memoria leyendo ejecutable */
imagen = crear_imagen(prog, &pc_inicial);
if (imagen)
{
	p_proc->info_mem = imagen;
	p_proc->pila = crear_pila(TAM_PILA);
	fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
		pc_inicial,
		&(p_proc->contexto_regs));
	p_proc->id = proc;
	p_proc->estado = LISTO;

	/* lo inserta al final de cola de listos */
	insertar_ultimo(&lista_listos, p_proc);
	error = 0;
}
else
error = -1; /* fallo al crear imagen */

return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

 /*
  * Tratamiento de llamada al sistema crear_proceso. Llama a la
  * funcion auxiliar crear_tarea sis_terminar_proceso
  */
int sis_crear_proceso() {
	char* prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog = (char*)leer_registro(1);
	res = crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char* texto;
	unsigned int longi;

	texto = (char*)leer_registro(1);
	longi = (unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso() {

	printk("-> FIN PROCESO %d\n", p_proc_actual->id);
	
	/////////
	//MUTEX//
	/////////
	if (p_proc_actual->n_descriptores > 0) {

		printk("Cerrar mutex que ha abierto el proceso\n");

		for (int i = 0; i < NUM_MUT_PROC; i++) {
			

			unsigned int des = p_proc_actual->descriptores[i];
			if (des != 0){
				
				
				cerrar_mutex(i);

			}
		}
		printk("Se ha terminado de cerrar los mutex\n");
	}
	
	liberar_proceso();

	return 0; /* no deber�a llegar aqui */
}

int obtener_id_pr() {
	int id = p_proc_actual->id;
	printk("ID del proceso actual es: %d\n", id);
	return id;
}

//////////////
/// DORMIR ///
//////////////

int dormir(unsigned int segundos){

	//leemos el parametro de los registros
	
	unsigned int segundosRegistros = (unsigned int)leer_registro(1);
	printk("Se procede a dormir %d segundos\n", segundosRegistros);
	
	//guardamos el nivel de interrupcion
	int nivelInterrupcion = fijar_nivel_int(NIVEL_3);
	BCPptr actual = p_proc_actual;

	//actualizamos la estructura de datos BCPptr
	actual->estado = BLOQUEADO;
	actual->segundosDormir = segundosRegistros * TICK;
	

	//cambiamos de lista el proceso actual
	eliminar_elem(&lista_listos, actual);
	insertar_ultimo(&lista_bloq_mutex, actual);

	//el planificador devuelve el proceso a ejecutar siguiente en la cola
	p_proc_actual = planificador();

	//restaurauramos el nivel de interrupcion
	fijar_nivel_int(nivelInterrupcion);

	cambio_contexto(&(actual->contexto_regs), &(p_proc_actual->contexto_regs));

	printk("Proceso %d termina de dormir.\n", p_proc_actual->id);
	return 0;
}

void cuentaAtrasBloqueados(){
	
	//printk("CUENTA ATRAS BLOQUEADOS COMIENZA\n");
	//recorro la lista y actualizo los tiempos
	
	BCPptr aux = lista_bloq_mutex.primero;
	int nivelInterrupcion = fijar_nivel_int(NIVEL_3);
	
	
	//printk("Proceso %d dormido durante %d ticks\n" ,aux->id ,aux->segundosDormir);
	
	
	while(aux != NULL){
	//printk("Entra al while!!!!!!!!!\n");
		//Fijamos el n de interrupcion para evitar problemas en la indexacion y eliminacion de procesos
		fijar_nivel_int(NIVEL_3);
		
		BCPptr siguiente = aux->siguiente;
		aux->segundosDormir--;
		
		if(aux->segundosDormir <=0){
			//si ha terminado lo cambio de lista
			aux->estado = LISTO;
			eliminar_elem(&lista_bloq_mutex, aux);
			insertar_ultimo(&lista_listos, aux);
		}
		
		aux = siguiente;
	}
	
	//Restauramos el n de interrupcion
	fijar_nivel_int(nivelInterrupcion);
	
	return;
}

///////////
// MUTEX //
///////////

//////// FUNCIONES AUXILIARES //////////
int descriptor_libre() {

	for (int i = 0; i < NUM_MUT_PROC; i++) {
		
		if (p_proc_actual->descriptores[i] == 0){
		
			p_proc_actual->descriptores[i]++;
			return i;
		}
	}

	return -1;
}

int nombres_iguales(char* nombre) {
	for (int i = 0; i < mutex_creados; i++) {

		//Si el mutex existe
		if (array_mutex[i].abierto != 0) {

			//Comprobamos si las cadenas son iguales
			if ((strcmp(nombre, array_mutex[i].nombre)) == 0) {

				//Da error si hay dos mutex con el mismo nombre
				return -1;
			}
		}
	}
	return 0; //Si no encuentra nombre igual
}

int descriptor_mutex() {

	int aux = -1;
	int i = 0;

	while ((aux == -1) && (i < NUM_MUT)) {

		//Si no ha sido abierto ninguna vez, esta disponible
		if (array_mutex[i].abierto == 0) {

			aux = i; //El descriptor es la posicion en el array
		}

		i++;
	}
	return aux;
}

////////// FUNCIONES PEDIDAS ////////////

int lock(unsigned int mutexid) {

	printk("SECCI�N LOCK\n");
	printk("Se procede a bloquear el mutex\n");

	//Recibe el descriptor
	unsigned int id = (unsigned int) leer_registro(1); 
	
	int encontrado= -1;
	for(int i = 0;i < NUM_MUT_PROC;i++){
	
		if(p_proc_actual->descriptores[i] == id) encontrado = 0;
	}
	
	if (encontrado == -1) {

		printk("El descriptor del mutex no existe. ERROR\n");
		return -1;
	}

	//Hacemos un bucle por si el proceso se queda bloqueado le esperamos
	int proceso_block = 0;

	while (proceso_block == 0) {

		//Si el mutex no se ha bloqueado a�n lo bloqueamos 
		if (array_mutex[id].locked == 0) {

			array_mutex[id].locked++;
			proceso_block = 1;
			
			printk("Lock realizado correctamente -> id del mutex = %d\n\n", id);
			return 0;
		}
		
		//Comprobamos si el mutex esta ya bloqueado y si es recursivo, en ese caso, se podria volver a bloquear
		if ((array_mutex[id].locked > 0) && (array_mutex[id].tipo == 1)) {


			printk("Id del propietario del mutex = %d. Id del proceso actual: %d\n", array_mutex[id].propietario, p_proc_actual->id);

			//Comprobamos si ha sido bloqueado por el proceso actual
			if (array_mutex[id].propietario == p_proc_actual->id) {

				
				//Si el proceso actual es el propiertario del mutex entonces lo volvemos a bloquear
				array_mutex[id].locked++;
				proceso_block = 1;
			}	
			//Si no es el propietario se bloquea el proceso actual
			else {

				printk("Este mutex ya esta bloqueado por otro proceso, se procede a bloquear el proceso actual\n");

				int nivel_int = fijar_nivel_int(NIVEL_3);

				//Cambiamos el estado del proceso a bloqueado
				p_proc_actual->estado = BLOQUEADO;

				//Lo eliminamos de la lista de procesos listos y lo ponemos en la lista de procesos bloqueados
				eliminar_primero(&lista_listos);
				insertar_ultimo(&array_mutex[id].lista_proc_esperando_lock, p_proc_actual);
				array_mutex[id].n_procesos_esperando++;

				//Conseguimos el nuevo proceso actual 
				BCP* p_proc_bloq = p_proc_actual;
				p_proc_actual = planificador();

				//Cambiamos el contexto
				cambio_contexto(&(p_proc_bloq->contexto_regs), &(p_proc_actual->contexto_regs));
				printk("Contexto cambiado del proceso: %d al proceso: %d\n", p_proc_bloq->id, p_proc_actual->id);

				//Recuperamos el nivel de interrupcion anterior
				fijar_nivel_int(nivel_int);
			}
		}

		//Comprobamos si el mutex esta bloqueado y es NO_RECURSIVO 
		if ((array_mutex[id].locked > 0) && (array_mutex[id].tipo == 0)) {

			//Comprobamos si ha sido bloqueado por el proceso actual
			if (array_mutex[id].propietario == p_proc_actual->id) {

				printk("El mutex que se quiere bloquear no es recursivo y ya ha sido bloqueado por este proceso. ERROR\n");
				return -1;
			}
			//Si el mutex ha sido bloqueado por un proceso que no es el actual
			else {

				printk("Este mutex ya esta bloqueado por otro proceso, se procede a bloquear el proceso actual\n");

				int nivel_int = fijar_nivel_int(NIVEL_3);

				//Cambiamos el estado del proceso a bloqueado
				p_proc_actual->estado = BLOQUEADO;

				//Lo eliminamos de la lista de procesos listos y lo ponemos en la lista de procesos bloqueados
				eliminar_primero(&lista_listos);
				insertar_ultimo(&array_mutex[id].lista_proc_esperando_lock, p_proc_actual);

				//Conseguimos el nuevo proceso actual 
				BCP* p_proc_bloq = p_proc_actual;
				p_proc_actual = planificador();

				//Cambiamos el contexto
				cambio_contexto(&(p_proc_bloq->contexto_regs), &(p_proc_actual->contexto_regs));
				printk("Contexto cambiado del proceso: %d al proceso: %d\n", p_proc_bloq->id, p_proc_actual->id);

				//Recuperamos el nivel de interrupcion anterior
				fijar_nivel_int(nivel_int);
			}
		}
	}

	printk("Lock realizado correctamente -> id del mutex = %d\n\n", id);
	return 0;
}

int unlock(unsigned int mutexid) {

	printk("SECCI�N UNLOCK\n");
	printk("Se procede a desbloquear el mutex\n");

	//Recibe el descriptor
	int descriptor_proceso = (unsigned int)leer_registro(1);

	//Guardamos el id del descriptor del mutex
	int id_mutex = p_proc_actual->descriptores[descriptor_proceso];


	//No se puede usar un mutex si este no esta abierto asi que debemos comprobar que lo est�
	if (array_mutex[descriptor_proceso].abierto == 0) {

		printk("Este mutex no ha sido abierto a�n. ERROR\n");
		return -1;
	}

	//Comprobamos que el mutex este bloqueado
	if (array_mutex[descriptor_proceso].locked == 0) {
		
		//Si no esta bloqueado da error
		printk("El mutex no se ha bloqueado, por tanto, no se puede desbloquear. ERROR\n");
		return -1;
	}
	//Si esta bloqueado entonces comprobamos 
	else {

		//Comprobamos si el mutex esta bloqueado y si es recursivo
		if ((array_mutex[descriptor_proceso].locked > 0) && (array_mutex[descriptor_proceso].tipo == RECURSIVO)) {

			//Comprobamos si ha sido bloqueado por el proceso actual y si es as� lo desbloqueamos
			if (array_mutex[descriptor_proceso].propietario == p_proc_actual->id) {

				//Si es el due�o lo desbloquea
				array_mutex[descriptor_proceso].locked--;

				//Hay que comprobar si el mutex sigue bloqueado
				if (array_mutex[descriptor_proceso].locked == 0) {

					//Comprobamos si existen procesos bloqueados en este mutex y si es as� lo desbloqueamos
					if (((array_mutex[descriptor_proceso].lista_proc_esperando_lock).primero) != NULL) {

						int nivel_int = fijar_nivel_int(NIVEL_3);

						//Cogemos el proceso que esta esperando y lo ponemos a listo
						BCP* proc_esperando = (array_mutex[descriptor_proceso].lista_proc_esperando_lock).primero;
						proc_esperando->estado = LISTO;

						//Lo eliminamos de la lista de procesos esperando y lo ponemos en la lista de listos
						eliminar_primero(&(array_mutex[descriptor_proceso].lista_proc_esperando_lock));
						insertar_ultimo(&lista_listos, proc_esperando);

						//Recuperamos el nivel de interrupci�n anterior
						fijar_nivel_int(nivel_int);
						
						printk("Se ha desbloqueado el proceso %d\n", proc_esperando->id);
					}

					//Una vez desbloqueado lo eliminamos de propietario
					array_mutex[descriptor_proceso].propietario = -1;
				} 
			}
			//Si el proceso actual no es el que lo bloque� no puede desbloquearlo
			else {

				printk("Este proceso no ha sido el bloqeuante del mutex, por lo tanto, no puede desbloquearlo. ERROR\n");
				return -1;
			}
		}

		//Comprobamos si el mutex esta bloqueado y si no es recursivo
		if ((array_mutex[descriptor_proceso].locked > 0) && (array_mutex[descriptor_proceso].tipo == NO_RECURSIVO)) {

			//Comprobamos que el mutex haya sido bloqueado por el proceso actual
			if (array_mutex[descriptor_proceso].propietario == p_proc_actual->id) {

				//Si es el due�o lo desbloquea
				array_mutex[descriptor_proceso].locked--;

				//Si alg�n proceso esta esperando por el mutex lo desbloqueamos
				if (((array_mutex[descriptor_proceso].lista_proc_esperando_lock).primero) != NULL) {

					int nivel_int = fijar_nivel_int(NIVEL_3);

					//Cogemos el proceso que esta esperando y lo ponemos a listo
					BCP* proc_esperando = (array_mutex[descriptor_proceso].lista_proc_esperando_lock).primero;
					proc_esperando->estado = LISTO;

					//Lo eliminamos de la lista de procesos esperando y lo ponemos en la lista de listos
					eliminar_primero(&(array_mutex[descriptor_proceso].lista_proc_esperando_lock));
					insertar_ultimo(&lista_listos, proc_esperando);

					//Recuperamos el nivel de interrupci�n anterior
					fijar_nivel_int(nivel_int);

					printk("Se ha desbloqueado el proceso %d\n", proc_esperando->id);
				}
			}
			else {

				printk("Este proceso no ha sido el bloqueante del mutex, por lo tanto, no puede desbloquearlo. ERROR\n");
				return -1;
			}
		}
	}

	printk("Unlock realizado correctamente -> id del mutex = %d\n\n", id_mutex);
	return 0;

}

int crear_mutex(char* nombre, int tipo) {

	printk("SECCI�N CREAR MUTEX\n");

	nombre= (char*)leer_registro(1);
	tipo = (int)leer_registro(2);
	
	printk("Tipo: %d\n", tipo);

	//Comprobamos si el nombre no excede los caracteres maximos
	if (strlen(nombre) > MAX_NOM_MUT) { 

		printk("El nombre del mutex es demasiado largo. ERROR\n");
		return -1;
	}

	//Comprobamos que haya descriptores libres y si lo hay guardamos la posicion del mismo
	int nuevo_des = descriptor_libre();
	if (nuevo_des == -1) {

		printk("El proceso no tiene descriptores libres. ERROR\n");
		return -1;
	}


	//Comprobamos que el nombre del mutex no sea igual a otro
	if (nombres_iguales(nombre) == -1) {

		printk("Ya existe un mutex con este nombre. ERROR\n");
		return -1;
	}


	while (mutex_creados == NUM_MUT) {

		printk("Alcanzado el maximo de mutex creados. Se va a bloquear el proceso hasta eliminar algun mutex\n");
		
		int nivel_int = fijar_nivel_int(NIVEL_3);

		//Cambiamos el estado del proceso a bloqueado
		p_proc_actual->estado = BLOQUEADO;

		//Lo eliminamos de la lista de procesos listos y lo ponemos en la lista de procesos bloqueados
		eliminar_primero(&lista_listos);
		insertar_ultimo(&lista_bloq_mutex, p_proc_actual); 

		//Conseguimos el nuevo proceso actual 
		BCP* p_proc_bloq = p_proc_actual; 
		p_proc_actual = planificador();

		//Cambiamos el contexto
		cambio_contexto(&(p_proc_bloq->contexto_regs), &(p_proc_actual->contexto_regs));
		printk("Se procede a cambiar el contexto del proceso: %d al proceso: %d\n", p_proc_bloq->id, p_proc_actual->id);

		//Recuperamos el nivel de interrupcion anterior
		fijar_nivel_int(nivel_int);
	}

	//Ahora ya se puede crear el mutex
	//Obtener descriptor de mutex libre para acceder a el
	int descriptor_mut = descriptor_mutex();

	//A�adir este descriptor al array de descriptores del proceso en la posicion correspondiente
	p_proc_actual->descriptores[nuevo_des] = descriptor_mut;
	printk("Descriptor_proc %d -> descr_mut = %d\n", nuevo_des, descriptor_mut);

	//Actualizar variables mutex
	array_mutex[descriptor_mut].nombre = nombre;
	array_mutex[descriptor_mut].tipo = tipo;
	array_mutex[descriptor_mut].abierto++;
	array_mutex[descriptor_mut].propietario = p_proc_actual->id;
	mutex_creados++;

	//Actualizar variables proceso
	p_proc_actual->n_descriptores++;

	printk("Mutex %s creado correctamente\n\n", nombre);

	//Devuelve el descriptor del mutex creado
	return descriptor_mut; 

}

int abrir_mutex(char* nombre) {

	printk("SECCI�N ABRIR_MUTEX\n");

	nombre = (char*)leer_registro(1);
	printk("Abriendo mutex %c\n", nombre);

	//Comprobamos que el nombre no supuere el tama�o maximo permitido
	if (strlen(nombre) > MAX_NOM_MUT) {

		printk("El nombre del mutex es demasiado largo. ERROR\n");
		return -1;
	}

	//Comprobamos que haya descriptores libres y si lo hay guardamos la posicion del mismo
	int nuevo_des = descriptor_libre();
	if (nuevo_des == -1) {

		printk("El proceso no tiene descriptores libres. ERROR\n");
		return -1;
	}

	//Comprobamos que el nombre del mutex sea igual a otro
	if (nombres_iguales(nombre) != -1) {

		printk("Ya existe un mutex con este nombre. ERROR\n");
		return -1;
	}

	//Iteramos todo el array de mutex creados para encontrar el mutex buscado
	int nom;
	for (int i = 0; i < mutex_creados; i++) {

		//Comprobamos en cada iteraci�n si los nombres son iguales
		if ((strcmp(nombre, array_mutex[i].nombre)) == 0) {

			//Una vez encontrado el mutex nos quedamos el descriptor
			nom = i;

		}
	}

	//Ahora tenemos que asignar el descriptor al proceso actual
	p_proc_actual->descriptores[nuevo_des] = nom;

	//Tenemos que actualizar tambi�n las variables del proceso
	p_proc_actual->n_descriptores++;
	array_mutex[nuevo_des].abierto++;

	printk("Se ha abierto el mutex correctamente\n");

	//Devuelve el descriptor del mutex abierto
	printk("Se devuelve %d\n\n", nuevo_des);
	return nuevo_des;
}

int cerrar_mutex(unsigned int mutexid) {

	printk("SECCI�N CERRAR_MUTEX\n");

	unsigned int id_mutex = (unsigned int)leer_registro(1);

	printk("id del mutex recibido: %d\n", mutexid);
	


	//Comprobamos que exista el mutex
	if (array_mutex[mutexid].nombre == NULL) {

		printk("El descriptor buscado no se ha encontrado. ERROR \n");
		return -1;
	}

	//Si lo encontramos, lo cerramos
	p_proc_actual->descriptores[mutexid] = -1;
	p_proc_actual->n_descriptores--;
	
	//Si el mutex no ha sido abierto por ningun proceso se quita directamente
	if (array_mutex[mutexid].abierto <= 0) {

		printk("El mutex %d ha sido eliminado\n", mutexid);
		mutex_creados--;

		//Si hay algun proceso esperando al mutex debido a que se hab�an creado el maximo de mutex
		//Hay que desbloquear los procesos bloqueados que estan esperando a este
		while (lista_bloq_mutex.primero != NULL) {

			int nivel_int = fijar_nivel_int(NIVEL_3);

			//Cogemos el proceso que esta esperando y lo ponemos a listo
			BCP* proc_esperando = lista_bloq_mutex.primero;
			proc_esperando->estado = LISTO;

			//Lo eliminamos de la lista de procesos bloqueados y lo ponemos en la lista de listos
			eliminar_primero(&lista_bloq_mutex);
			insertar_ultimo(&lista_listos, proc_esperando);

			//Recuperamos el nivel de interrupci�n anterior
			fijar_nivel_int(nivel_int);

			printk("Se ha desbloqueado el proceso %d\n", proc_esperando->id);
		}
	}

	//Si el mutex a cerrar esta bloaqueado, hay que desbloquearlo
	if (array_mutex[mutexid].propietario == p_proc_actual->id) {

		printk("Se procede a desbloquear el mutex\n");
		array_mutex[mutexid].locked = 0;

		//Si hay algun proceso esperando al mutex por lock, hay que desbloquearlo
		while ((array_mutex[mutexid].lista_proc_esperando_lock).primero != NULL) {

			int nivel_int = fijar_nivel_int(NIVEL_3);

			//Cogemos el proceso que esta esperando y lo ponemos a listo
			BCP* proc_esperando = (array_mutex[mutexid].lista_proc_esperando_lock).primero;
			proc_esperando->estado = LISTO;

			//Lo eliminamos de la lista de procesos esperando y lo ponemos en la lista de listos
			eliminar_primero(&(array_mutex[mutexid].lista_proc_esperando_lock));
			insertar_ultimo(&lista_listos, proc_esperando);

			//Recuperamos el nivel de interrupci�n anterior
			fijar_nivel_int(nivel_int);

			printk("Se ha desbloqueado el proceso %d\n", proc_esperando->id);
		}
	}

	array_mutex[mutexid].abierto--;
	printk("El mutex %d ha sido cerrado correctamente\n", mutexid);

	return 0;
}
int liberarTodosLosProcesosBloqueadosMutex(mutex* m){

	printk("Liberando procesos bloqueados en el mutex %s.\n", m->nombre);
	
	BCPptr actual = m->lista_proc_esperando_lock.primero;
	BCPptr siguiente = NULL;
	
	while(m->lista_proc_esperando_lock.primero != NULL){
	
		siguiente = actual->siguiente;
		
		int nivel_int = fijar_nivel_int(NIVEL_3);
		
		eliminar_primero(&m->lista_proc_esperando_lock);
		insertar_ultimo(&lista_listos, siguiente);
		
		actual->estado = LISTO;
		
		fijar_nivel_int(nivel_int);
		
		actual = siguiente;
	}
	// redundante, pero viene bien para deteccion de errores fuera del scope de esta funcion
	return m->lista_proc_esperando_lock.primero == NULL ?  1 :  -1;
}

/*
 *
 * Rutina de inicializaci�n invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	
	return 0;
}
