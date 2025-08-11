/**
 * @file klib/spinlock/spinlock.h
 * @brief Primitive di sincronizzazione a spinlock per ZONE-OS
 *
 * Fornisce uno spinlock minimalista per sezioni critiche del kernel.
 * Usa intrinseci atomici (__atomic) con semantica acquire/release, un hint
 * di attesa attiva (arch_cpu_pause).
 *
 * Implementazione piattaforma-indipendente in klib/spinlock/spinlock.c.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once

#include <lib/stdbool.h>
#include <lib/types.h> /* u32 */

/* Dimensione cacheline (se non definita altrove) */
#ifndef ARCH_CACHELINE_SIZE
#define ARCH_CACHELINE_SIZE 64
#endif

/**
 * @brief Struttura dati per un lock a spin
 */
typedef struct __attribute__((aligned(ARCH_CACHELINE_SIZE))) {
  volatile u32 locked; /**< 1 se lock acquisito, 0 se libero */
} spinlock_t;

/**
 * @brief Inizializzatore statico per spinlock
 */
#define SPINLOCK_INITIALIZER {.locked = 0}

/**
 * @brief Inizializza uno spinlock
 * @param lock Puntatore allo spinlock da inizializzare
 */
void spinlock_init(spinlock_t *lock);

/**
 * @brief Tenta di acquisire il lock senza bloccare
 * @param lock Spinlock da acquisire
 * @return true se acquisito, false altrimenti
 */
bool spinlock_trylock(spinlock_t *lock);

/**
 * @brief Acquisisce il lock con attesa attiva
 * @param lock Spinlock da acquisire
 */
void spinlock_lock(spinlock_t *lock);

/**
 * @brief Rilascia lo spinlock
 * @param lock Spinlock da rilasciare
 */
void spinlock_unlock(spinlock_t *lock);
