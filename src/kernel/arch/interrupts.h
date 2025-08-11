#pragma once
#include <lib/stdint.h>

typedef void (*arch_isr_t)(void *frame);

/**
 * @brief Imposta i vettori di interrupt / exception.
 */
void arch_interrupts_init(void);

/**
 * @brief Registra un handler per un vettore specifico.
 */
void arch_register_isr(int vector, arch_isr_t fn);
