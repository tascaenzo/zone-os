/**
 * @file klib/bitmap.h
 * @brief Bitmap utility per gestione bit in strutture kernel
 *
 * Fornisce primitive per manipolare bitmap generiche, usate ad esempio in
 * memory allocator, slab allocator, gestori di ID, e tracking risorse.
 */

#pragma once

#include <lib/stdbool.h>
#include <lib/types.h>

/* -------------------------------------------------------------------------- */
/*                               TIPO ASTRATTO                                */
/* -------------------------------------------------------------------------- */

typedef struct {
  u64 *bits;        /* Puntatore al buffer di bit */
  size_t bit_count; /* Numero totale di bit gestiti */
} bitmap_t;

/* -------------------------------------------------------------------------- */
/*                               API PUBBLICA                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Inizializza una bitmap esistente
 *
 * @param bm Puntatore alla bitmap da inizializzare
 * @param storage Puntatore al buffer di memoria preallocata (allineato a 8 byte)
 * @param bit_count Numero totale di bit da gestire
 */
void bitmap_init(bitmap_t *bm, u64 *storage, size_t bit_count);

/**
 * @brief Imposta a 1 il bit specificato
 *
 * @param bm Puntatore alla bitmap
 * @param index Indice del bit da settare
 */
void bitmap_set(bitmap_t *bm, size_t index);

/**
 * @brief Azzera (0) il bit specificato
 */
void bitmap_clear(bitmap_t *bm, size_t index);

/**
 * @brief Legge il valore del bit (0 o 1)
 */
bool bitmap_get(bitmap_t *bm, size_t index);

/**
 * @brief Trova il primo bit a 0 (libero)
 *
 * @return indice del primo bit a 0, oppure -1UL se nessun bit libero trovato
 */
size_t bitmap_find_first_clear(bitmap_t *bm);

/**
 * @brief Trova un intervallo di bit a 0 contigui
 *
 * @param bm Bitmap da cercare
 * @param count Lunghezza dell'intervallo
 * @return Indice iniziale dell'intervallo o -1UL se non trovato
 */
size_t bitmap_find_clear_run(bitmap_t *bm, size_t count);

/**
 * @brief Riempie tutta la bitmap a 0
 */
void bitmap_clear_all(bitmap_t *bm);

/**
 * @brief Imposta tutti i bit a 1
 */
void bitmap_set_all(bitmap_t *bm);
