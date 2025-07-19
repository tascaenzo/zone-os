#pragma once

/*
 * NULL: il valore per un puntatore nullo.
 * Viene definito come (void*)0 per supportare il confronto con altri tipi di puntatore.
 */
#define NULL ((void *)0)

/*
 * size_t: tipo unsigned per rappresentare dimensioni e conteggi.
 * Usato comunemente per array, buffer e offset.
 * Su x86_64 corrisponde a unsigned long (64 bit).
 */
typedef unsigned long size_t;

/*
 * ptrdiff_t: tipo signed per rappresentare la differenza tra due puntatori
 * della stessa array. Può essere negativo, quindi è signed.
 */
typedef long ptrdiff_t;

/*
 * offsetof: macro per calcolare l'offset del membro 'member' all'interno
 * della struttura 'type'. Funziona creando un puntatore a NULL, quindi non
 * accede a memoria valida, ma calcola solo l'indirizzo offset.
 *
 * Esempio:
 *   struct Foo { int a; char b; };
 *   size_t off = offsetof(struct Foo, b);
 *   // off == 4 su architettura con allineamento 4 byte per int
 */
#define offsetof(type, member) ((size_t)&(((type *)0)->member))
