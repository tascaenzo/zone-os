/**
 * @file mm/memory.h
 * @brief Interfaccia memoria core (arch-indipendente) per ZONE-OS
 *
 * Definisce i tipi e le funzioni usate dal kernel per la gestione
 * della mappa memoria. L’implementazione interna si basa sulle API
 * arch-specifiche dichiarate in <arch/memory.h>, ma senza esporre
 * dettagli dell’architettura.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once

#include <lib/stdbool.h>
#include <lib/stddef.h>
#include <lib/types.h>

/**
 * @brief Tipologie di memoria gestite dal core.
 */
typedef enum {
  MEMORY_USABLE = 0,             /**< Memoria fisica utilizzabile */
  MEMORY_RESERVED,               /**< Riservata, non allocabile */
  MEMORY_ACPI_RECLAIMABLE,       /**< ACPI reclaimable */
  MEMORY_ACPI_NVS,               /**< ACPI NVS */
  MEMORY_BAD,                    /**< Bad memory */
  MEMORY_BOOTLOADER_RECLAIMABLE, /**< Rilasciabile dal bootloader */
  MEMORY_EXECUTABLE_AND_MODULES, /**< Kernel e moduli caricati */
  MEMORY_FRAMEBUFFER,            /**< Framebuffer video */
  MEMORY_MMIO,                   /**< Memory-mapped I/O */
  MEMORY_TYPE_COUNT              /**< Numero totale di tipi */
} memory_type_t;

/**
 * @brief Struttura che descrive una regione di memoria fisica.
 */
typedef struct {
  u64 base;           /**< Indirizzo fisico base */
  u64 length;         /**< Lunghezza in byte */
  memory_type_t type; /**< Tipologia di memoria */
} memory_region_t;

/**
 * @brief Statistiche globali della memoria rilevata.
 */
typedef struct {
  u64 total_memory;        /**< Memoria fisica totale */
  u64 usable_memory;       /**< Memoria utilizzabile */
  u64 reserved_memory;     /**< Memoria riservata */
  u64 executable_memory;   /**< Memoria occupata da kernel/moduli */
  u64 largest_free_region; /**< Dimensione max di regione USABLE contigua */
} memory_stats_t;

/**
 * @brief Inizializza il sottosistema memoria (arch + logica).
 */
void memory_init(void);

/**
 * @brief Completa l'inizializzazione dopo che lo heap è pronto.
 */
void memory_late_init(void);

/**
 * @brief Stampa la mappa memoria rilevata (solo debug).
 */
void memory_print_map(void);

/**
 * @brief Restituisce un puntatore alle statistiche globali calcolate.
 *
 * @return Puntatore valido fintanto che il kernel è in esecuzione.
 */
const memory_stats_t *memory_get_stats(void);

/**
 * @brief Cerca la regione USABLE più grande rilevata.
 *
 * @param base Puntatore dove salvare l'indirizzo base.
 * @param length Puntatore dove salvare la dimensione.
 * @return true se trovata, false altrimenti.
 */
bool memory_find_largest_region(u64 *base, u64 *length);

/**
 * @brief Verifica se una regione è completamente contenuta in memoria USABLE.
 *
 * @param base Indirizzo base.
 * @param length Lunghezza della regione.
 * @return true se l'intera regione è usabile.
 */
bool memory_is_region_usable(u64 base, u64 length);

/**
 * @brief Restituisce il puntatore alla mappa memoria corrente (read-only).
 *
 * @param count Puntatore dove salvare il numero di regioni (può essere NULL).
 * @return Puntatore costante alla lista di regioni.
 */
const memory_region_t *memory_regions(size_t *count);

/**
 * @brief Restituisce il numero di regioni nella mappa corrente.
 */
size_t memory_region_count(void);

/**
 * @brief Copia la mappa memoria corrente in un buffer fornito.
 *
 * @param out Buffer di destinazione.
 * @param max Numero massimo di regioni copiabili.
 * @return Numero di regioni effettivamente copiate.
 */
size_t memory_copy_regions(memory_region_t *out, size_t max);
