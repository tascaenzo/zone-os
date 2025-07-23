#include <klib/klog.h>
#include <lib/string.h>
#include <mm/memory.h>

#define MAX_REGIONS 512 // Limite massimo di regioni supportate nel kernel

// Array contenente tutte le regioni di memoria rilevate
static memory_region_t regions[MAX_REGIONS];

// Numero di regioni effettivamente valide rilevate
static size_t region_count = 0;

// Statistiche aggregate della memoria del sistema
static memory_stats_t stats;

/**
 * @brief Inizializza il sottosistema memoria completo
 *
 * - Inizializza l'interfaccia architetturale
 * - Rileva le regioni di memoria fisica
 * - Calcola statistiche globali
 */
void memory_init(void) {
  klog_info("[mem] Inizializzazione sottosistema memoria");

  arch_memory_init();

  region_count = arch_memory_detect_regions(regions, MAX_REGIONS);
  if (region_count == 0) {
    klog_panic("[mem] Nessuna regione valida rilevata");
  }

  arch_memory_get_stats(&stats);

  klog_info("[mem] %zu regioni rilevate, totale: %lu MiB", region_count, stats.total_memory / (1024 * 1024));
}

/**
 * @brief Stampa la mappa di memoria rilevata (per debug)
 */
void memory_print_map(void) {
  klog_info("=== MEMORY MAP ===");
  for (size_t i = 0; i < region_count; i++) {
    memory_region_t *r = &regions[i];
    klog_info("[%02zu] 0x%016lx - 0x%016lx  %6lu KiB  type=%d", i, r->base, r->base + r->length - 1, r->length / 1024, r->type);
  }
  klog_info("===================");
}

/**
 * @brief Ritorna le statistiche globali di memoria calcolate
 */
const memory_stats_t *memory_get_stats(void) {
  return &stats;
}

/**
 * @brief Trova la regione USABLE più grande presente nella mappa
 *
 * @param base[out]   Base della regione trovata
 * @param length[out] Dimensione della regione
 * @return true se trovata una regione valida, false altrimenti
 */
bool memory_find_largest_region(u64 *base, u64 *length) {
  u64 max = 0;
  u64 best_base = 0;

  for (size_t i = 0; i < region_count; i++) {
    memory_region_t *r = &regions[i];
    if (r->type == MEMORY_USABLE && r->length > max) {
      max = r->length;
      best_base = r->base;
    }
  }

  if (max > 0) {
    if (base)
      *base = best_base;
    if (length)
      *length = max;
    return true;
  }

  return false;
}

/**
 * @brief Verifica se una regione è interamente contenuta in regioni USABLE
 *
 * @param base Indirizzo di partenza
 * @param length Lunghezza della regione
 * @return true se completamente contenuta in regioni USABLE
 */
bool memory_is_region_usable(u64 base, u64 length) {
  u64 end = base + length;

  for (size_t i = 0; i < region_count; i++) {
    memory_region_t *r = &regions[i];
    if (r->type != MEMORY_USABLE)
      continue;

    u64 r_end = r->base + r->length;
    if (base >= r->base && end <= r_end) {
      return true;
    }
  }

  return false;
}
