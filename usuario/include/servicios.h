/*
 *  usuario/include/servicios.h
 *
 *  Minikernel. Versión 1.0
 *
 *  Fernando Pérez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene los prototipos de funciones de
 * biblioteca que proporcionan la interfaz de llamadas al sistema.
 *
 *      SE DEBE MODIFICAR AL INCLUIR NUEVAS LLAMADAS
 *
 */

#ifndef SERVICIOS_H
#define SERVICIOS_H

/* Evita el uso del printf de la bilioteca estándar */
#define printf escribirf


// DORMIR

int dormir(unsigned int segundos);
void cuentaAtrasBloqueados();
/////////////////////////
// Servicios del mutex //
/////////////////////////
#define NO_RECURSIVO 0
#define RECURSIVO 1

int crear_mutex(char* nombre, int tipo);
int abrir_mutex(char* nombre);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);
int cerrar_mutex(unsigned int mutexid);

////////////////
// AUXILIARES //
////////////////

int descriptor_libre();
int nombres_iguales(char* nombre);
int descriptor_mutex();

/////////////////////////////////////////

/* Funcion de biblioteca */
int escribirf(const char *formato, ...);

/* Llamadas al sistema proporcionadas */
int crear_proceso(char *prog);
int terminar_proceso();
int escribir(char *texto, unsigned int longi);
int obtener_id_pr();


#endif /* SERVICIOS_H */

