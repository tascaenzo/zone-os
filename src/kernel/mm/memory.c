#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <klib/klog.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/types.h>
#include <mm/memory.h>

// Variabili globali definite nell'header
u64 memory_map_entries = 0;
memory_stats_t memory_stats;

// Richiesta memory map per Limine
volatile struct limine_memmap_request memmap_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

// Funzione per convertire i tipi Limine ai nostri tipi
static memory_type_t convert_limine_type(uint64_t limine_type) {
  switch (limine_type) {
  case LIMINE_MEMMAP_USABLE:
    return MEMORY_USABLE;
  case LIMINE_MEMMAP_RESERVED:
    return MEMORY_RESERVED;
  case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
    return MEMORY_ACPI_RECLAIMABLE;
  case LIMINE_MEMMAP_ACPI_NVS:
    return MEMORY_ACPI_NVS;
  case LIMINE_MEMMAP_BAD_MEMORY:
    return MEMORY_BAD;
  case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
    return MEMORY_BOOTLOADER_RECLAIMABLE;
  case LIMINE_MEMMAP_KERNEL_AND_MODULES:
    return MEMORY_EXECUTABLE_AND_MODULES;
  case LIMINE_MEMMAP_FRAMEBUFFER:
    return MEMORY_FRAMEBUFFER;
  default:
    return MEMORY_RESERVED;
  }
}

// Ottiene il nome del tipo di memoria
static const char *memory_type_name(memory_type_t type) {
  switch (type) {
  case MEMORY_USABLE:
    return "USABLE";
  case MEMORY_RESERVED:
    return "RESERVED";
  case MEMORY_ACPI_RECLAIMABLE:
    return "ACPI_RECLAIMABLE";
  case MEMORY_ACPI_NVS:
    return "ACPI_NVS";
  case MEMORY_BAD:
    return "BAD";
  case MEMORY_BOOTLOADER_RECLAIMABLE:
    return "BOOTLOADER_RECLAIMABLE";
  case MEMORY_EXECUTABLE_AND_MODULES:
    return "EXECUTABLE_AND_MODULES";
  case MEMORY_FRAMEBUFFER:
    return "FRAMEBUFFER";
  default:
    return "UNKNOWN";
  }
}

void memory_init(void) {
  klog_info("Inizializzazione sottosistema memoria...");

  // Verifica che il bootloader abbia fornito la memory map
  if (!memmap_request.response) {
    klog_panic("Il bootloader non ha fornito la memory map");
  }

  struct limine_memmap_response *response = memmap_request.response;
  memory_map_entries = response->entry_count;

  if (memory_map_entries == 0) {
    klog_panic("Memory map vuota dal bootloader");
  }

  if (memory_map_entries > 1000) {
    klog_panic("Memory map entries troppo alte: possibile corruzione");
  }

  klog_info("Memory map contiene %lu voci", (unsigned long)memory_map_entries);

  // Reset delle statistiche
  memset(&memory_stats, 0, sizeof(memory_stats));

  // Analizza ogni regione di memoria
  for (u64 i = 0; i < memory_map_entries; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    memory_type_t type = convert_limine_type(entry->type);

    // Aggiorna memoria totale
    memory_stats.total_memory += entry->length;

    // Aggiorna contatori per categoria
    switch (type) {
    case MEMORY_USABLE:
      memory_stats.usable_memory += entry->length;

      // Traccia la regione libera più grande
      if (entry->length > memory_stats.largest_free_region) {
        memory_stats.largest_free_region = entry->length;
      }
      break;

    case MEMORY_EXECUTABLE_AND_MODULES:
      memory_stats.executable_memory += entry->length;
      break;

    case MEMORY_BOOTLOADER_RECLAIMABLE:
      // Il bootloader reclaimable può essere recuperato dopo il boot
      memory_stats.usable_memory += entry->length;
      break;

    case MEMORY_ACPI_RECLAIMABLE:
      // ACPI reclaimable può essere recuperato dopo aver letto le tabelle ACPI
      memory_stats.usable_memory += entry->length;
      break;

    case MEMORY_RESERVED:
    case MEMORY_ACPI_NVS:
    case MEMORY_BAD:
    case MEMORY_FRAMEBUFFER:
      // Queste sono davvero riservate e non recuperabili
      memory_stats.reserved_memory += entry->length;
      break;

    default:
      memory_stats.reserved_memory += entry->length;
      break;
    }

    klog_debug("Regione %lu: 0x%016lx-0x%016lx (%lu MB) %s", (unsigned long)i, (unsigned long)entry->base, (unsigned long)(entry->base + entry->length - 1),
               (unsigned long)(entry->length / (1024 * 1024)), memory_type_name(type));
  }

  // Log del riepilogo
  klog_info("Analisi memoria completata:");
  klog_info("  Memoria totale:      %lu MB", (unsigned long)(memory_stats.total_memory / (1024 * 1024)));
  klog_info("  Memoria usabile:     %lu MB", (unsigned long)(memory_stats.usable_memory / (1024 * 1024)));
  klog_info("  Memoria riservata:   %lu MB", (unsigned long)(memory_stats.reserved_memory / (1024 * 1024)));

  // Mostra memoria eseguibile in KB se è < 1MB
  if (memory_stats.executable_memory < 1024 * 1024) {
    klog_info("  Memoria eseguibile:  %lu KB", (unsigned long)(memory_stats.executable_memory / 1024));
  } else {
    klog_info("  Memoria eseguibile:  %lu MB", (unsigned long)(memory_stats.executable_memory / (1024 * 1024)));
  }

  klog_info("  Regione piu' grande: %lu MB", (unsigned long)(memory_stats.largest_free_region / (1024 * 1024)));

  // Verifica i calcoli
  u64 calculated_total = memory_stats.usable_memory + memory_stats.reserved_memory + memory_stats.executable_memory;
  if (calculated_total != memory_stats.total_memory) {
    klog_warn("Discrepanza nei calcoli memoria: totale=%lu, calcolato=%lu", (unsigned long)(memory_stats.total_memory / (1024 * 1024)),
              (unsigned long)(calculated_total / (1024 * 1024)));
  }

  // Verifica che abbiamo memoria sufficiente
  if (memory_stats.usable_memory < 16 * 1024 * 1024) { // Meno di 16MB
    klog_warn("Memoria usabile molto bassa: %lu MB", (unsigned long)(memory_stats.usable_memory / (1024 * 1024)));
  }

  klog_info("Sottosistema memoria inizializzato con successo");
}

void memory_print_map(void) {
  if (!memmap_request.response) {
    klog_error("Memory map non disponibile");
    return;
  }

  struct limine_memmap_response *response = memmap_request.response;

  klog_info("=== MEMORY MAP ===");
  klog_info("Totale regioni: %lu", (unsigned long)memory_map_entries);

  for (u64 i = 0; i < memory_map_entries; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    memory_type_t type = convert_limine_type(entry->type);

    klog_info("Regione %lu: 0x%016lx-0x%016lx (%lu MB) %s", (unsigned long)i, (unsigned long)entry->base, (unsigned long)(entry->base + entry->length - 1),
              (unsigned long)(entry->length / (1024 * 1024)), memory_type_name(type));
  }
  klog_info("=================");
}

const memory_stats_t *memory_get_stats(void) {
  return &memory_stats;
}

bool memory_find_largest_region(u64 *base, u64 *length) {
  if (!memmap_request.response || !base || !length) {
    return false;
  }

  struct limine_memmap_response *response = memmap_request.response;
  u64 largest_size = 0;
  u64 largest_base = 0;

  for (u64 i = 0; i < memory_map_entries; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    memory_type_t type = convert_limine_type(entry->type);

    if (type == MEMORY_USABLE && entry->length > largest_size) {
      largest_size = entry->length;
      largest_base = entry->base;
    }
  }

  if (largest_size > 0) {
    *base = largest_base;
    *length = largest_size;
    return true;
  }

  return false;
}

bool memory_is_region_usable(u64 base, u64 length) {
  if (!memmap_request.response) {
    return false;
  }

  struct limine_memmap_response *response = memmap_request.response;
  u64 end = base + length;

  for (u64 i = 0; i < memory_map_entries; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    memory_type_t type = convert_limine_type(entry->type);

    if (type == MEMORY_USABLE) {
      u64 entry_end = entry->base + entry->length;

      // Verifica se la regione richiesta è completamente contenuta in questa regione usabile
      if (base >= entry->base && end <= entry_end) {
        return true;
      }
    }
  }

  return false;
}