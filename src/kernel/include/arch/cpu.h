#pragma once
#include <lib/types.h>

/**
 * @file cpu.h
 * @brief Interfaccia architettura-agnostica per operazioni CPU a basso livello
 *
 * Questo header espone un set minimo di primitive CPU utilizzabili dal kernel
 * senza esporre dettagli specifici dell’architettura (x86_64, RISC-V, ARM...).
 *
 * Ogni funzione viene implementata in arch/<arch>/cpu.c
 */

/*
 * ============================================================================
 * CPUID / FEATURE DETECTION
 * ============================================================================
 */

/**
 * @brief Verifica se la CPU supporta il bit NX (No-Execute)
 *
 * @return true se NX è supportato via CPUID
 */
bool cpu_supports_nx(void);

/**
 * @brief Verifica se la CPU supporta syscall/sysret
 *
 * @return true se disponibile la syscall ABI
 */
bool cpu_supports_syscall(void);

/*
 * ============================================================================
 * CONTROL REGISTER / MSR ACCESS
 * ============================================================================
 */

/**
 * @brief Legge un Model Specific Register (MSR)
 *
 * @param msr ID del registro (es. 0xC0000080 per EFER)
 * @return Valore a 64-bit del MSR
 */
u64 cpu_rdmsr(u32 msr);

/**
 * @brief Scrive un valore in un MSR
 *
 * @param msr ID del registro
 * @param value Valore a 64-bit da scrivere
 */
void cpu_wrmsr(u32 msr, u64 value);

/**
 * @brief Legge il contenuto del registro CR3 (Page Table Base)
 *
 * @return Valore corrente di CR3 (fisico)
 */
u64 cpu_read_cr3(void);

/**
 * @brief Scrive CR3 per cambiare spazio virtuale
 *
 * @param cr3_val Valore fisico da caricare in CR3
 */
void cpu_write_cr3(u64 cr3_val);

/**
 * @brief Invalida una singola entry nel TLB
 *
 * @param virt_addr Indirizzo virtuale da invalidare
 */
void cpu_invlpg(u64 virt_addr);

/*
 * ============================================================================
 * MSR-BASED CONFIGURAZIONE
 * ============================================================================
 */

/**
 * @brief Abilita il bit NXE nel registro EFER
 *
 * Deve essere chiamato dopo aver verificato che NX sia supportato.
 */
void cpu_enable_nxe_bit(void);

/*
 * ============================================================================
 * HALT / IDLE / INTERRUPT CONTROL
 * ============================================================================
 */

/**
 * @brief Blocca la CPU finché non riceve un interrupt
 */
void cpu_halt(void);

/**
 * @brief Abilita globalmente gli interrupt (STI)
 */
void cpu_enable_interrupts(void);

/**
 * @brief Disabilita globalmente gli interrupt (CLI)
 */
void cpu_disable_interrupts(void);

/*
 * ============================================================================
 * IDENTIFICAZIONE CPU
 * ============================================================================
 */

/**
 * @brief Ottiene una stringa descrittiva dell’architettura corrente
 *
 * @return Stringa costante (es. "x86_64")
 */
const char *cpu_get_arch_name(void);

/**
 * @brief Ottiene una stringa descrittiva del vendor CPU
 *
 * @return Es: "GenuineIntel", "AuthenticAMD", etc.
 */
const char *cpu_get_vendor(void);
