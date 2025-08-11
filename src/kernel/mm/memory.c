/**
 * @file mm/memory.c
 * @brief Memory subsystem core (arch-indipendente) per ZONE-OS
 *
 * Questo modulo implementa la logica di gestione della memoria fisica
 * indipendente dall’architettura. Si appoggia al layer HAL dichiarato
 * in <arch/memory.h> per ottenere la mappa fisica e le statistiche
 * fornite dalla piattaforma, e le converte nei tipi e strutture
 * utilizzati dal kernel core.
 *
 * Funzionalità principali:
 *  - Inizializzazione e late-init del sottosistema memoria
 *  - Conversione e memorizzazione della mappa fisica in formato core
 *  - Calcolo di statistiche globali (totale, utilizzabile, riservata)
 *  - Query e utility per ricerca e validazione di regioni
 *  - Debug-print della mappa memoria
 *
 * Questo codice è completamente arch-agnostico: l’unico legame con
 * l’architettura è l’uso delle API <arch/memory.h>.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include "memory.h"
#include <arch/memory.h>
#include <klib/klog/klog.h>
#include <lib/string/string.h>
#include <limine.h>
#include <mm/heap/heap.h>
/**
 * @brief Richiesta memory map a Limine
 *
 * Questa è la struttura che Limine popola automaticamente durante il boot
 * con informazioni complete sulla memoria fisica disponibile.
 * È specifica per x86_64 e bootloader Limine.
 */
volatile struct limine_memmap_request memmap_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

/* Puntatore alle regioni attualmente in uso */
// Temporary memory region array for initialization
static memory_region_t temp_regions[ARCH_MAX_MEMORY_REGIONS];
static memory_region_t *g_regions = temp_regions;

/* Numero di regioni effettivamente valide rilevate */
static size_t g_region_count = 0;

/* Statistiche aggregate della memoria del sistema */
static memory_stats_t g_stats;

/**
 * @brief Mapping tipo arch → tipo core.
 */
static inline memory_type_t map_arch_type(arch_mem_type_t t) {
  switch (t) {
  case ARCH_MEM_USABLE:
    return MEMORY_USABLE;
  case ARCH_MEM_RESERVED:
    return MEMORY_RESERVED;
  case ARCH_MEM_ACPI_RECLAIM:
    return MEMORY_ACPI_RECLAIMABLE;
  case ARCH_MEM_ACPI_NVS:
    return MEMORY_ACPI_NVS;
  case ARCH_MEM_BAD:
    return MEMORY_BAD;
  case ARCH_MEM_BOOT_RECLAIM:
    return MEMORY_BOOTLOADER_RECLAIMABLE;
  case ARCH_MEM_KERNEL:
    return MEMORY_EXECUTABLE_AND_MODULES;
  case ARCH_MEM_FRAMEBUFFER:
    return MEMORY_FRAMEBUFFER;
  case ARCH_MEM_MMIO:
    return MEMORY_MMIO;
  default:
    return MEMORY_RESERVED;
  }
}

/**
 * @brief Ricostruisce le statistiche globali dal vettore g_regions.
 */
static void recompute_stats(void) {
  memset(&g_stats, 0, sizeof(g_stats));
  for (size_t i = 0; i < g_region_count; ++i) {
    const memory_region_t *r = &g_regions[i];
    g_stats.total_memory += r->length;

    switch (r->type) {
    case MEMORY_USABLE:
    case MEMORY_BOOTLOADER_RECLAIMABLE:
    case MEMORY_ACPI_RECLAIMABLE:
      g_stats.usable_memory += r->length;
      if (r->type == MEMORY_USABLE && r->length > g_stats.largest_free_region)
        g_stats.largest_free_region = r->length;
      break;
    case MEMORY_EXECUTABLE_AND_MODULES:
      g_stats.executable_memory += r->length;
      break;
    default:
      g_stats.reserved_memory += r->length;
      break;
    }
  }
}

/**
 * @brief Inizializza il sottosistema memoria (arch + logica).
 */
void memory_init(void) {
  klog_info("[mem] init...");

  arch_memory_init();

  /* Rileva con HAL in buffer intermedio arch-agnostico e converte nel core type */
  arch_mem_region_t arch_buf[ARCH_MAX_MEMORY_REGIONS];
  size_t n = arch_memory_detect_regions(arch_buf, ARCH_MAX_MEMORY_REGIONS);
  if (n == 0)
    klog_panic("[mem] nessuna regione valida rilevata");

  g_region_count = 0;
  for (size_t i = 0; i < n; ++i) {
    const arch_mem_region_t *a = &arch_buf[i];
    memory_region_t r = {
        .base = a->base,
        .length = a->length,
        .type = map_arch_type(a->type),
    };
    g_regions[g_region_count++] = r;
  }

  /* Statistiche dal core (comprehensive) */
  recompute_stats();

  /* Log sintetico */
  klog_info("[mem] %zu regioni, totale=%lu MiB, usable=%lu MiB", g_region_count, g_stats.total_memory / (1024UL * 1024UL), g_stats.usable_memory / (1024UL * 1024UL));
}

/**
 * @brief Completa l'inizializzazione dopo che lo heap è pronto.
 */
void memory_late_init(void) {
  if (g_regions != temp_regions)
    return;

  memory_region_t *dyn = kmalloc(g_region_count * sizeof(memory_region_t));
  if (!dyn)
    klog_panic("[mem] alloc fallita per tabella regioni");

  memcpy(dyn, temp_regions, g_region_count * sizeof(memory_region_t));
  g_regions = dyn;
  klog_info("[mem] tabella regioni spostata nello heap (%zu voci)", g_region_count);
}

/**
 * @brief Stampa la mappa memoria rilevata (debug).
 */
void memory_print_map(void) {
  klog_info("=== MEMORY MAP ===");
  for (size_t i = 0; i < g_region_count; ++i) {
    const memory_region_t *r = &g_regions[i];
    klog_info("[%02zu] 0x%016lx - 0x%016lx  %6lu KiB  type=%d", i, r->base, r->base + r->length - 1, r->length / 1024UL, r->type);
  }
  klog_info("===================");
}

/**
 * @brief Restituisce un puntatore alle statistiche globali calcolate.
 *
 * @return Puntatore valido fintanto che il kernel è in esecuzione.
 */
const memory_stats_t *memory_get_stats(void) {
  return &g_stats;
}

/**
 * @brief Cerca la regione USABLE più grande rilevata.
 *
 * @param base Indirizzo base (out).
 * @param length Lunghezza della regione (out).
 * @return true se trovata, false altrimenti.
 */
bool memory_find_largest_region(u64 *base, u64 *length) {
  u64 max = 0, best = 0;
  for (size_t i = 0; i < g_region_count; ++i) {
    const memory_region_t *r = &g_regions[i];
    if (r->type == MEMORY_USABLE && r->length > max) {
      max = r->length;
      best = r->base;
    }
  }
  if (max) {
    if (base)
      *base = best;
    if (length)
      *length = max;
    return true;
  }
  return false;
}

/**
 * @brief Verifica se una regione è completamente contenuta in memoria USABLE.
 *
 * @param base Indirizzo base.
 * @param length Lunghezza della regione.
 * @return true se l'intera regione è usabile.
 */
bool memory_is_region_usable(u64 base, u64 length) {
  const u64 end = base + length;
  if (end < base)
    return false; /* overflow */

  for (size_t i = 0; i < g_region_count; ++i) {
    const memory_region_t *r = &g_regions[i];
    if (r->type != MEMORY_USABLE)
      continue;
    const u64 rend = r->base + r->length;
    if (base >= r->base && end <= rend)
      return true;
  }
  return false;
}

/**
 * @brief Restituisce il puntatore alla mappa memoria corrente (read-only).
 *
 * @param count Numero di regioni (out, può essere NULL).
 * @return Puntatore costante alla lista di regioni.
 */
const memory_region_t *memory_regions(size_t *count) {
  if (count)
    *count = g_region_count;
  return g_regions;
}

/**
 * @brief Restituisce il numero di regioni nella mappa corrente.
 */
size_t memory_region_count(void) {
  return g_region_count;
}

/**
 * @brief Copia la mappa memoria corrente in un buffer fornito.
 *
 * @param out Buffer di destinazione.
 * @param max Numero massimo di regioni copiabili.
 * @return Numero di regioni effettivamente copiate.
 */
size_t memory_copy_regions(memory_region_t *out, size_t max) {
  if (!out || max == 0)
    return 0;
  const size_t n = (g_region_count < max) ? g_region_count : max;
  memcpy(out, g_regions, n * sizeof(memory_region_t));
  return n;
}
