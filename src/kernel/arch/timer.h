#pragma once
#include <lib/stdint.h>

/**
 * @brief Inizializza il timer di sistema (PIT, APIC, ARM timers).
 */
void arch_timer_init(void);

/**
 * @brief Restituisce i tick monotoni trascorsi.
 */
uint64_t arch_timer_ticks(void);

/**
 * @brief Attende per un dato numero di millisecondi (busy-wait o simile).
 */
void arch_timer_sleep_ms(uint64_t ms);
