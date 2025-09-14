/**
 * @file arch/memory.h
 * @brief Layer di astrazione della memoria fisica (arch-specific, discovery + HHDM)
 *
 * Interfaccia arch-agnostica per:
 *  - Discovery della memoria fisica via firmware/bootloader
 *  - Statistiche aggregate sulle regioni fisiche
 *  - Validazione di intervalli fisici secondo vincoli dell’architettura
 *  - Accesso opzionale a HHDM (Higher Half Direct Map)
 *
 * Implementazioni: arch/<arch>/memory/memory_arch.c
 * NOTE:
 *  - Non include primitive di VMM/paging (vedi arch/.../paging.h).
 *  - I formati/flag del firmware sono convertiti in tipi canonici del kernel.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once
#include <lib/stdbool.h> /* bool   */
#include <lib/stddef.h>  /* size_t */
#include <lib/stdint.h>  /* uintptr_t */
#include <lib/types.h>   /* u64, ssize_t (atteso), ecc. */

typedef enum {
  MEMORY_USABLE = 0,
  MEMORY_RESERVED,
  MEMORY_ACPI_RECLAIMABLE,
  MEMORY_ACPI_NVS,
  MEMORY_BAD,
  MEMORY_BOOTLOADER_RECLAIMABLE,
  MEMORY_EXECUTABLE_AND_MODULES,
  MEMORY_FRAMEBUFFER,
  MEMORY_TYPE_COUNT
} memory_type_t;

typedef struct {
  u64 base;           /* Indirizzo fisico iniziale */
  u64 length;         /* Lunghezza in byte         */
  memory_type_t type; /* Tipo canonico             */
} memory_region_t;

typedef struct {
  u64 total_memory;
  u64 usable_memory;
  u64 reserved_memory;
  u64 executable_memory;
  u64 largest_free_region;
} memory_stats_t;

/* ============================================================================
 *  PHYSICAL MEMORY DISCOVERY (arch-specific)
 * ==========================================================================*/

/**
 * @brief Inizializza il sottosistema memoria arch-specific (probe/sanity).
 *        Va chiamata prima di qualsiasi altra API di questo header.
 */
void arch_memory_init(void);

/**
 * @brief Enumera le regioni fisiche e le converte in formato canonico.
 *
 * @param[out] out  Array di destinazione (fornito dal chiamante).
 * @param[in]  max  Numero massimo di entry scrivibili in @p out.
 * @return >=0: numero di regioni scritte; <0: -errno su errore.
 */
size_t arch_memory_detect_regions(memory_region_t *out, size_t max);

/**
 * @brief Restituisce le statistiche aggregate dell’ultima detect_regions().
 *
 * @param[out] out  Struttura destinazione (non NULL).
 */
void arch_memory_get_stats(memory_stats_t *out);

/**
 * @brief Verifica che l’intervallo fisico [base, base+length) sia valido
 *        per la piattaforma corrente (limiti, allineamenti, aree riservate).
 *
 * @param base    Indirizzo fisico di partenza.
 * @param length  Lunghezza in byte.
 * @return true se l’intervallo è valido per l’arch, false altrimenti.
 */
bool arch_memory_region_valid(u64 base, u64 length);

/* ============================================================================
 *  HHDM (Higher Half Direct Map)
 * ==========================================================================*/

/**
 * @brief Restituisce la base dell’HHDM (offset VA-PA) se presente.
 *
 * @return Offset HHDM oppure 0 se assente/non abilitato.
 */
u64 arch_hhdm_base(void);

/**
 * @brief Indica se l’HHDM è attivo e utilizzabile.
 */
bool arch_hhdm_available(void);

/**
 * @brief Verifica che l’intervallo fisico sia interamente mappato in HHDM.
 *
 * @return true se [base, base+length) è mappato nell’HHDM, altrimenti false.
 */
bool arch_hhdm_maps(u64 base, u64 length);

/**
 * @brief Helper PHYS→VIRT via HHDM (no-op se HHDM assente).
 *        Il chiamante dovrebbe verificare arch_hhdm_available()/arch_hhdm_maps().
 */
static inline void *arch_phys_to_virt(u64 pa) {
  u64 base = arch_hhdm_base();
  if (!base)
    return (void *)(uintptr_t)pa;
  /* opzionale: proteggere overflow se necessario */
  return (void *)(uintptr_t)(pa + base);
}

/**
 * @brief Helper VIRT→PHYS via HHDM (no-op se HHDM assente).
 */
static inline u64 arch_virt_to_phys(const void *va) {
  u64 base = arch_hhdm_base();
  return (u64)(uintptr_t)(base ? ((uintptr_t)va - base) : (uintptr_t)va);
}
