/**
 * @file klib/list.h
 * @brief Doubly Linked List - Generic Kernel Utility
 *
 * Interfaccia per lista doppiamente concatenata generica utilizzabile nel kernel.
 * Separata dall'implementazione (in list.c), non richiede allocazioni dinamiche
 * ed è pensata per strutture embedded (slab, scheduler, ecc).
 */

#pragma once

#include <lib/stdbool.h>
#include <lib/types.h>

/* -------------------------------------------------------------------------- */
/*                          Struttura base del nodo                           */
/* -------------------------------------------------------------------------- */

typedef struct list_node {
  struct list_node *prev;
  struct list_node *next;
} list_node_t;

/* -------------------------------------------------------------------------- */
/*                              Funzioni disponibili                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Inizializza un nodo lista come lista vuota (sentinella)
 *
 * Imposta i puntatori prev e next del nodo in modo che puntino a sé stesso,
 * rendendolo una lista circolare vuota valida.
 *
 * @param node Puntatore al nodo da inizializzare
 */
void list_init(list_node_t *node);

/**
 * @brief Inserisce un nodo subito dopo un nodo esistente
 *
 * Collega il nuovo nodo immediatamente dopo il nodo specificato, aggiornando
 * correttamente i puntatori prev e next dei nodi coinvolti.
 *
 * @param pos Nodo esistente dopo il quale inserire il nuovo nodo
 * @param new_node Nodo da inserire
 */
void list_insert_after(list_node_t *pos, list_node_t *new_node);

/**
 * @brief Inserisce un nodo subito prima di un nodo esistente
 *
 * Collega il nuovo nodo immediatamente prima del nodo specificato, aggiornando
 * correttamente i puntatori prev e next dei nodi coinvolti.
 *
 * @param pos Nodo esistente prima del quale inserire il nuovo nodo
 * @param new_node Nodo da inserire
 */
void list_insert_before(list_node_t *pos, list_node_t *new_node);

/**
 * @brief Rimuove un nodo dalla lista
 *
 * Aggiorna i puntatori dei nodi adiacenti per mantenere la coerenza della
 * struttura circolare della lista. Il nodo rimosso viene reimpostato in stato
 * isolato (prev/next puntano a sé stesso).
 *
 * @param node Nodo da rimuovere
 */
void list_remove(list_node_t *node);

/**
 * @brief Verifica se la lista è vuota
 *
 * Controlla se il nodo head ha come next sé stesso, indicando una lista
 * senza elementi.
 *
 * @param list Puntatore al nodo sentinella
 * @return true se la lista è vuota, false altrimenti
 */
bool list_is_empty(const list_node_t *list);

/* -------------------------------------------------------------------------- */
/*                         Macro di utilità per iterazione                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Itera su tutti gli elementi della lista
 *
 * @param it Variabile iteratore
 * @param head Lista su cui iterare
 */
#define LIST_FOR_EACH(it, head) for ((it) = (head)->next; (it) != (head); (it) = (it)->next)

/**
 * @brief Itera su tutti gli elementi in modo sicuro durante la modifica
 *
 * @param it Variabile iteratore corrente
 * @param tmp Variabile temporanea
 * @param head Lista su cui iterare
 */
#define LIST_FOR_EACH_SAFE(it, tmp, head) for ((it) = (head)->next, (tmp) = (it)->next; (it) != (head); (it) = (tmp), (tmp) = (it)->next)

/**
 * @brief Ottiene il container da un campo nodo
 *
 * @param ptr Puntatore al campo list_node dentro la struttura
 * @param type Tipo della struttura contenente
 * @param member Nome del campo list_node
 */
#define LIST_ENTRY(ptr, type, member) ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))
