#pragma once

#include <lib/types.h>

/**
 * @file klib/spinlock.h
 * @brief Primitive di sincronizzazione a spinlock
 *
 * Fornisce un semplice spinlock basato su operazioni atomiche
 * GCC/Clang. Utilizzato per proteggere sezioni critiche del kernel
 * in ambienti multiprocessore.
 */

/**
 * @brief Struttura dati per un lock a spin
 */
typedef struct {
  volatile u32 locked; /**< 1 se lock acquisito, 0 se libero */
} spinlock_t;

/**
 * @brief Inizializzatore statico per spinlock
 */
#define SPINLOCK_INITIALIZER {0}

/**
 * @brief Inizializza uno spinlock
 * @param lock Puntatore allo spinlock da inizializzare
 */
__attribute__((unused)) static inline void spinlock_init(spinlock_t *lock) {
  lock->locked = 0;
}

/**
 * @brief Acquisisce il lock con attesa attiva
 * @param lock Spinlock da acquisire
 */
__attribute__((unused)) static inline void spinlock_lock(spinlock_t *lock) {
  while (__sync_lock_test_and_set(&lock->locked, 1)) {
    while (lock->locked) {
      __asm__ volatile("pause");
    }
  }
}

/**
 * @brief Rilascia lo spinlock
 * @param lock Spinlock da rilasciare
 */
__attribute__((unused)) static inline void spinlock_unlock(spinlock_t *lock) {
  __sync_lock_release(&lock->locked);
}