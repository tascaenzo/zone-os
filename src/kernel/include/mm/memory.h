#pragma once
#include <lib/types.h>

// Tipi di regioni di memoria da Limine v9.x
typedef enum {
  MEMORY_USABLE = 0,
  MEMORY_RESERVED = 1,
  MEMORY_ACPI_RECLAIMABLE = 2,
  MEMORY_ACPI_NVS = 3,
  MEMORY_BAD = 4,
  MEMORY_BOOTLOADER_RECLAIMABLE = 5,
  MEMORY_EXECUTABLE_AND_MODULES = 6,
  MEMORY_FRAMEBUFFER = 7,
} memory_type_t;

typedef struct {
  u64 base;           // Indirizzo fisico di inizio
  u64 length;         // Dimensione in byte
  memory_type_t type; // Tipo di regione di memoria
} memory_region_t;

// Statistiche della memoria
typedef struct {
  u64 total_memory;        // Memoria totale rilevata
  u64 usable_memory;       // Memoria disponibile per uso
  u64 reserved_memory;     // Memoria riservata da hardware/bootloader
  u64 executable_memory;   // Memoria usata dall'eseguibile (kernel) e moduli
  u64 largest_free_region; // Regione libera contigua più grande
} memory_stats_t;

// Informazioni globali sulla memoria
extern u64 memory_map_entries;
extern memory_stats_t memory_stats;

/**
 * @brief Inizializza il sottosistema memoria e analizza la memory map
 */
void memory_init(void);

/**
 * @brief Stampa informazioni dettagliate sulla memory map
 */
void memory_print_map(void);

/**
 * @brief Ottiene le statistiche della memoria
 * @return Puntatore alla struttura delle statistiche
 */
const memory_stats_t *memory_get_stats(void);

/**
 * @brief Trova la regione di memoria utilizzabile più grande
 * @param base Puntatore per salvare l'indirizzo base
 * @param length Puntatore per salvare la lunghezza della regione
 * @return true se trovata, false se nessuna regione utilizzabile
 */
bool memory_find_largest_region(u64 *base, u64 *length);

/**
 * @brief Verifica se un range di indirizzi fisici è utilizzabile
 * @param base Indirizzo fisico di partenza
 * @param length Lunghezza della regione da verificare
 * @return true se l'intera regione è utilizzabile
 */
bool memory_is_region_usable(u64 base, u64 length);