/**
 * @file libk/bitmap.c
 * @brief Bitmap - Implementazione
 *
 * Gestione di bitmap generiche con operazioni bitwise efficienti.
 */

#include "bitmap.h"
#include <lib/string/string.h>

#define BITMAP_WORD_BITS 64
#define BITMAP_WORD_INDEX(i) ((i) / BITMAP_WORD_BITS)
#define BITMAP_BIT_OFFSET(i) ((i) % BITMAP_WORD_BITS)

/* Inizializza la struttura bitmap e la azzera completamente */
void bitmap_init(bitmap_t *bm, u64 *storage, size_t bit_count) {
  bm->bits = storage;
  bm->bit_count = bit_count;
  bitmap_clear_all(bm);
}

/* Imposta a 1 il bit in posizione index */
void bitmap_set(bitmap_t *bm, size_t index) {
  if (index >= bm->bit_count)
    return;
  bm->bits[BITMAP_WORD_INDEX(index)] |= (1ULL << BITMAP_BIT_OFFSET(index));
}

/* Azzera a 0 il bit in posizione index */
void bitmap_clear(bitmap_t *bm, size_t index) {
  if (index >= bm->bit_count)
    return;
  bm->bits[BITMAP_WORD_INDEX(index)] &= ~(1ULL << BITMAP_BIT_OFFSET(index));
}

/* Ritorna true se il bit è settato, false altrimenti */
bool bitmap_get(bitmap_t *bm, size_t index) {
  if (index >= bm->bit_count)
    return false;
  return (bm->bits[BITMAP_WORD_INDEX(index)] >> BITMAP_BIT_OFFSET(index)) & 1;
}

/* Cerca il primo bit libero (a 0) nella bitmap */
size_t bitmap_find_first_clear(bitmap_t *bm) {
  size_t word_count = (bm->bit_count + 63) / 64;
  for (size_t w = 0; w < word_count; ++w) {
    if (bm->bits[w] != ~0ULL) {
      for (size_t b = 0; b < 64; ++b) {
        size_t i = w * 64 + b;
        if (i >= bm->bit_count)
          return -1UL;
        if (!(bm->bits[w] & (1ULL << b)))
          return i;
      }
    }
  }
  return -1UL;
}

/* Cerca un run contiguo di bit liberi di lunghezza count */
size_t bitmap_find_clear_run(bitmap_t *bm, size_t count) {
  if (count == 0 || count > bm->bit_count)
    return -1UL;

  size_t run_start = 0;
  size_t run_len = 0;

  for (size_t i = 0; i < bm->bit_count; ++i) {
    if (!bitmap_get(bm, i)) {
      if (run_len == 0)
        run_start = i;
      run_len++;
      if (run_len == count)
        return run_start;
    } else {
      run_len = 0;
    }
  }

  return -1UL;
}

/* Azzera l'intera bitmap (tutti i bit a 0) */
void bitmap_clear_all(bitmap_t *bm) {
  size_t words = (bm->bit_count + 63) / 64;
  for (size_t i = 0; i < words; ++i)
    bm->bits[i] = 0ULL;
}

/* Imposta tutti i bit della bitmap a 1 */
void bitmap_set_all(bitmap_t *bm) {
  size_t words = (bm->bit_count + 63) / 64;
  for (size_t i = 0; i < words; ++i)
    bm->bits[i] = ~0ULL;
}
