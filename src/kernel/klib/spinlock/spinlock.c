/**
 * @file klib/spinlock/spinlock.c
 * @brief Implementazione degli spinlock per ZONE-OS
 *
 * Implementa spinlock con __atomic (acquire/release) e hint arch_cpu_pause().
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include "spinlock.h"
#include <arch/cpu.h> /* arch_cpu_pause(), (opz) arch_cpu_irq_save/restore */

void spinlock_init(spinlock_t *lock) {
  __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELAXED);
}

bool spinlock_trylock(spinlock_t *lock) {
  /* Acquire: se il precedente era 0, abbiamo preso il lock */
  return __atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE) == 0u;
}

void spinlock_lock(spinlock_t *lock) {
  /* Tentativo veloce */
  if (__atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE) == 0u)
    return;

  /* Contesa: spin su load rilassato + PAUSE per ridurre la pressione su bus */
  do {
    while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED)) {
      arch_cpu_pause();
    }
  } while (__atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE));
}

void spinlock_unlock(spinlock_t *lock) {
  /* Release: rende visibili le scritture nella sezione critica */
  __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE);
}
