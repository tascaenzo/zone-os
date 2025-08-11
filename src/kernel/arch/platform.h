/**
 * @file arch/platform.h
 * @brief API di inizializzazione e informazioni architetturali per ZONE-OS
 *
 * Fornisce funzioni generiche per identificare l'architettura in uso
 * e inizializzare i componenti fondamentali (GDT, IDT, timer, IRQ, ecc.)
 * in modo portabile, demandando i dettagli al backend specifico in arch/<arch>/platform.c
 *
 * @author Enzo Tasca
 * @date 2025
 * @license MIT
 */

#pragma once
#include <lib/stdint.h>

/**
 * @brief Restituisce il nome dell'architettura corrente.
 * @return Puntatore a stringa costante (es. "x86_64", "armv8").
 */
const char *arch_get_name(void);

/**
 * @brief Inizializza i componenti base dell'architettura.
 *
 * Comprende:
 *  - Setup tabelle di descrizione (GDT/IDT/TSS o equivalenti)
 *  - Configurazione base timer/clock
 *  - Inizializzazione sistema di interrupt
 *  - Inizializzazione memoria di basso livello
 *
 * Deve essere invocata una sola volta all'avvio del kernel.
 */
void arch_init(void);
