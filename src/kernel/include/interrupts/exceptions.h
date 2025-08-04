#pragma once

/**
 * @file interrupts/exceptions.h
 * @brief Inizializzazione e gestione delle eccezioni CPU (0–31)
 *
 * Questo modulo fornisce il setup dei gestori per tutte le eccezioni
 * standard dell'architettura x86_64. Le eccezioni sono gestite con
 * severità diversa (fatali o non fatali), a seconda del vettore.
 *
 * Il file `exceptions.c` implementa i dettagli, compresa la gestione
 * del Page Fault con supporto VMM.
 *
 * Da chiamare una volta durante l'avvio del kernel.
 */

#include <arch/interrupt_context.h>
#include <lib/types.h>

/**
 * @brief Inizializza e registra i gestori di eccezione CPU (INT 0–31)
 *
 * Deve essere chiamato dopo `interrupts_init()` e prima dell'esecuzione
 * normale del kernel. Gestisce:
 * - Page Fault (#PF)
 * - Breakpoint (#BP)
 * - FPU/SIMD faults
 * - Eccezioni generiche/fatali
 */
void exceptions_init(void);
