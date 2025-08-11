/**
 * @file arch/cpu.h
 * @brief CPU architecture abstraction layer for ZONE-OS
 *
 * Questo file definisce l'interfaccia comune per operazioni basilari
 * sulla CPU, indipendenti dall'architettura specifica (x86_64, ARM, RISC-V...).
 * Il kernel deve utilizzare queste funzioni invece di chiamare istruzioni
 * assembly specifiche direttamente, garantendo così portabilità tra
 * differenti piattaforme.
 *
 * Funzionalità incluse:
 *  - Controllo dello stato della CPU (halt, pause)
 *  - Gestione globale degli interrupt
 *  - Recupero di informazioni hardware di base
 *  - Sincronizzazione e memory barrier
 *
 * Implementazione:
 *   Ogni architettura fornisce la propria versione in:
 *   `arch/<arch>/cpu_impl.c`
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once
#include <lib/stdbool.h>
#include <lib/stdint.h>

/**
 * @brief Arresta la CPU (istruzioni tipiche: hlt, wfi).
 *        Utilizzato quando non ci sono compiti da eseguire.
 */
void arch_cpu_halt(void);

/**
 * @brief Abilita l'accettazione di interrupt hardware globalmente.
 */
void arch_cpu_enable_interrupts(void);

/**
 * @brief Disabilita globalmente l'accettazione di interrupt hardware.
 */
void arch_cpu_disable_interrupts(void);

/**
 * @brief Hint efficiente per spin-wait in loop attivi (pause, yield).
 *        Usato nei lock o nelle attese attive.
 */
void arch_cpu_pause(void);

/**
 * @brief Restituisce il numero di core CPU disponibili nel sistema.
 */
unsigned int arch_cpu_count(void);

/**
 * @brief Restituisce l'ID del core corrente (es. APIC ID, hartid).
 */
unsigned int arch_cpu_current_id(void);

/**
 * @brief Forza un flush delle cache (invalida L1/L2 se supportato).
 */
void arch_cpu_flush_cache(void);

/**
 * @brief Memory barrier: garantisce ordine di esecuzione delle operazioni
 *        di memoria per evitare riorganizzazioni da parte della CPU.
 */
void arch_cpu_memory_barrier(void);

/**
 * @brief Sincronizzazione tra core: barrier globale.
 *        Tipico per SMP (multi-core) in fase di avvio.
 */
void arch_cpu_sync_barrier(void);
