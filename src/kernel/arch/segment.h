/**
 * @file arch/segment.h
 * @brief API di inizializzazione della segmentazione per ZONE-OS
 *
 * Fornisce un'astrazione portabile per l'inizializzazione della segmentazione
 * dell'architettura corrente. Su x86/x86_64 include il setup della GDT e del TSS
 * e il caricamento dei segment register; su altre architetture può essere un no-op
 * o configurare strutture equivalenti.
 *
 * Questa API è progettata per essere invocata dal kernel in fase di bootstrap,
 * demandando la logica specifica al backend arch/<arch>/segment.c.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once
#include <lib/stdint.h>

/**
 * @brief Inizializza la segmentazione dell'architettura.
 *
 * Esegue tutte le operazioni necessarie per impostare e attivare la
 * segmentazione di base del processore.
 *
 * @note Va chiamata una sola volta in fase di early boot, prima di usare
 *       API che dipendono da stack per-CPU, TSS o gestione IRQ.
 */
void arch_segment_init(void);
