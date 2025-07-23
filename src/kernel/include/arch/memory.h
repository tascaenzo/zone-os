#pragma once

/**
 * @file arch/memory.h
 * @brief Interfaccia architetturale per la gestione memoria fisica
 *
 * Questo header definisce i tipi comuni (tipi di memoria, regioni fisiche,
 * statistiche) e la serie di funzioni che ogni architettura deve implementare
 * per consentire la gestione astratta della memoria fisica nel kernel.
 *
 * L'implementazione concreta per ciascuna architettura si trova in:
 * - arch/x86_64/memory.c
 * - arch/arm64/memory.c
 * - arch/riscv64/memory.c
 * - arch/i386/memory.c
 *
 * L'inclusione condizionale automatica dei file di paging è effettuata
 * per assicurare la disponibilità delle costanti/macro base per ogni arch.
 */

#include <lib/types.h>

// Inclusione automatica delle definizioni di paging arch-specifiche
#if defined(__x86_64__) || defined(_M_X64)
#include <arch/x86_64/paging_defs.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arch/arm64/paging_defs.h>
#elif defined(__riscv) && (__riscv_xlen == 64)
#include <arch/riscv64/paging_defs.h>
#elif defined(__i386__) || defined(_M_IX86)
#include <arch/i386/paging_defs.h>
#else
#error "Architettura non supportata. Aggiungi l'header corrispondente."
#endif

/* -------------------------------------------------------------------------- */
/*                         TIPI ASTRATTI DI MEMORIA                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Tipi canonici di regioni di memoria fisica
 *
 * Questi valori sono usati dal kernel e derivati dai valori forniti
 * dal bootloader (Limine, UEFI, Device Tree, ecc).
 */
typedef enum {
  MEMORY_USABLE = 0,             // RAM disponibile per uso kernel
  MEMORY_RESERVED,               // Riservata da firmware o dispositivi
  MEMORY_ACPI_RECLAIMABLE,       // Tabelle ACPI recuperabili
  MEMORY_ACPI_NVS,               // ACPI Non-Volatile (permanente)
  MEMORY_BAD,                    // Blocchi danneggiati fisicamente
  MEMORY_BOOTLOADER_RECLAIMABLE, // Usata dal bootloader, recuperabile
  MEMORY_EXECUTABLE_AND_MODULES, // Kernel o moduli caricati
  MEMORY_FRAMEBUFFER,            // Framebuffer video
  MEMORY_TYPE_COUNT              // Sentinella: numero totale tipi
} memory_type_t;

/**
 * @brief Rappresentazione di una singola regione fisica
 */
typedef struct {
  u64 base;           // Indirizzo fisico di partenza
  u64 length;         // Dimensione in byte
  memory_type_t type; // Tipo della regione
} memory_region_t;

/**
 * @brief Statistiche aggregate della memoria del sistema
 *
 * Questi valori vengono calcolati in fase di boot
 * e possono essere interrogati dal kernel.
 */
typedef struct {
  u64 total_memory;        // RAM fisica totale
  u64 usable_memory;       // Memoria recuperabile (USABLE + reclaimable)
  u64 reserved_memory;     // Memoria non utilizzabile
  u64 executable_memory;   // Area contenente kernel e moduli
  u64 largest_free_region; // Regione USABLE contigua più grande
} memory_stats_t;

/* -------------------------------------------------------------------------- */
/*                   INTERFACCIA ARCHITETTURALE DA IMPLEMENTARE              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Inizializzazione specifica per l'architettura corrente
 *
 * Verifica la disponibilità della memory map e imposta i prerequisiti
 * richiesti prima di eseguire qualunque altra operazione memoria.
 * Deve eseguire panic() in caso di errore fatale.
 */
void arch_memory_init(void);

/**
 * @brief Rileva e restituisce la lista delle regioni fisiche disponibili
 *
 * @param regions      Array da riempire con le regioni rilevate
 * @param max_regions  Numero massimo di slot disponibili nell'array
 * @return Numero effettivo di regioni valide rilevate
 *         0 se non è possibile leggere la memory map
 */
size_t arch_memory_detect_regions(memory_region_t *regions, size_t max_regions);

/**
 * @brief Valida una regione secondo i vincoli dell'architettura
 *
 * @param base    Indirizzo di inizio
 * @param length  Lunghezza della regione in byte
 * @return true se l'intervallo è valido, false altrimenti
 */
bool arch_memory_region_valid(u64 base, u64 length);

/**
 * @brief Restituisce una stringa identificativa dell'architettura
 * @return Stringa statica (es. "x86_64", "ARM64", etc.)
 */
const char *arch_get_name(void);

/**
 * @brief Calcola e restituisce le statistiche di memoria fisica
 *
 * Le statistiche vengono calcolate sulla base delle regioni rilevate
 * con arch_memory_detect_regions().
 *
 * @param stats Puntatore alla struttura da riempire (obbligatorio)
 */
void arch_memory_get_stats(memory_stats_t *stats);
