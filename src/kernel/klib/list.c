#include <klib/list.h>

/**
 * @file klib/list.c
 * @brief Doubly Linked List - Implementation
 *
 * Implementazione delle primitive per liste doppiamente concatenate generiche
 * utilizzabili nel kernel. Nessuna dipendenza da allocatori o strutture esterne.
 */

/* -------------------------------------------------------------------------- */
/*                               IMPLEMENTAZIONE                              */
/* -------------------------------------------------------------------------- */

/**
 * Inizializza un nodo come lista circolare vuota.
 */
void list_init(list_node_t *node) {
  node->next = node;
  node->prev = node;
}

/**
 * Inserisce un nuovo nodo subito dopo quello specificato.
 */
void list_insert_after(list_node_t *pos, list_node_t *new_node) {
  new_node->next = pos->next;
  new_node->prev = pos;
  pos->next->prev = new_node;
  pos->next = new_node;
}

/**
 * Inserisce un nuovo nodo subito prima di quello specificato.
 */
void list_insert_before(list_node_t *pos, list_node_t *new_node) {
  new_node->prev = pos->prev;
  new_node->next = pos;
  pos->prev->next = new_node;
  pos->prev = new_node;
}

/**
 * Rimuove un nodo dalla lista, ricollegando i vicini.
 */
void list_remove(list_node_t *node) {
  node->next->prev = node->prev;
  node->prev->next = node->next;
  node->next = node;
  node->prev = node;
}

/**
 * Verifica se la lista è vuota (solo sentinella).
 */
bool list_is_empty(const list_node_t *list) {
  return list->next == list;
}
