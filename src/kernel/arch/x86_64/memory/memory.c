/**
 * @file    arch/x86_64/memory.c
 * @brief   Implementazione x86_64 dell'interfaccia arch/memory.h (discovery + HHDM)
 *
 * RESPONSABILITÀ:
 *  - Ottenere la memory map dal bootloader Limine
 *  - Convertire i tipi Limine in tipi canonici del kernel
 *  - Validare regioni secondo i vincoli x86_64
 *  - Calcolare statistiche aggregate
 *  - Inizializzare il sottosistema memoria per x86_64
 *
 * NOTE:
 *  - Questo modulo non fa mapping VMM; si limita al discovery/validazione fisica.
 *  - HHDM e altre funzionalità runtime sono esposte via arch/memory.h (se implementate altrove).
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include "paging_defs.h"
#include "vmm_defs.h"

#include <arch/memory.h>
#include <arch/platform.h>
#include <arch/x86_64/cpu/cpu_lowlevel.h>

#include <klib/klog/klog.h>

#include <lib/string/string.h>
#include <lib/types.h>

#include <limine.h>
#include <mm/memory.h>

#include <lib/errno.h>
#include <lib/stdbool.h>
#include <lib/stddef.h>
#include <lib/stdint.h>

/* ============================================================================
 * COSTANTI E DEFINIZIONI x86_64
 * ==========================================================================*/

/* x86_64: indirizzi fisici fino a 52 bit (implementazioni tipiche 36–48). */
#define X86_64_MAX_PHYSICAL_BITS 52u
#define X86_64_MAX_PHYSICAL_ADDR ((1ull << X86_64_MAX_PHYSICAL_BITS) - 1ull)

/* Protezione pagina 0 (NULL pointer). */
#define X86_64_RESERVED_PAGE_0 0x1000ull

/* Soglia minima memoria utilizzabile consigliata. */
#define MIN_USABLE_MEMORY_MB 16u

/* Fallback locale se KCFG_MAX_MEM_REGIONS non è fornita dal kernel config. */
#ifndef KCFG_MAX_MEM_REGIONS
#define KCFG_MAX_MEM_REGIONS 512
#endif

typedef struct {
  u64 base;
  u64 end;
} mem_range_t;

/* Intervalli MMIO comuni (PC). */
static const mem_range_t mmio_ranges[] = {
    {0x000A0000ull, 0x000FFFFFull}, /* VGA/BIOS */
    {0xFEC00000ull, 0xFEEFFFFFull}, /* IOAPIC/HPET */
    {0xFE000000ull, 0xFEFFFFFFull}, /* Firmware   */
};
#define MMIO_RANGE_COUNT (sizeof(mmio_ranges) / sizeof(mmio_ranges[0]))

/* Intervalli ACPI/firmware comunemente riservati. */
static const mem_range_t acpi_reserved_ranges[] = {
    {0x000E0000ull, 0x000FFFFFull}, /* ACPI RSDP / BIOS */
};
#define ACPI_RANGE_COUNT (sizeof(acpi_reserved_ranges) / sizeof(acpi_reserved_ranges[0]))

/* ============================================================================
 * STATO INTERNO
 * ==========================================================================*/

/* Richiesta memory map a Limine (popolata dal bootloader). */
volatile struct limine_memmap_request memmap_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

/* Cache statistiche aggregate (populate in detect_regions). */
static memory_stats_t cached_stats = {.total_memory = 0, .usable_memory = 0, .reserved_memory = 0, .executable_memory = 0, .largest_free_region = 0};

static bool stats_valid = false;

/* ============================================================================
 * UTILITÀ INTERNE
 * ==========================================================================*/

static inline bool ranges_overlap(u64 a_start, u64 a_end, u64 b_start, u64 b_end) {
  return !(a_end < b_start || b_end < a_start);
}

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
    return "KERNEL_AND_MODULES";
  case MEMORY_FRAMEBUFFER:
    return "FRAMEBUFFER";
  default:
    return "UNKNOWN";
  }
}

static inline void log_memory_type(memory_type_t type) {
  klog_debug("memtype=%s", memory_type_name(type));
}

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
    klog_warn("x86_64: Limine mem type sconosciuto: %llu -> RESERVED", (unsigned long long)limine_type);
    return MEMORY_RESERVED;
  }
}

static bool validate_memory_region(const memory_region_t *region) {
  if (!region) {
    klog_debug("validate_memory_region(): NULL");
    return false;
  }

  /* overflow base+length */
  if (region->length == 0 || region->base + region->length < region->base) {
    klog_warn("x86_64: regione overflow/zero: base=0x%llx len=0x%llx", (unsigned long long)region->base, (unsigned long long)region->length);
    return false;
  }

  const u64 end_addr = region->base + region->length - 1ull;

  /* limiti fisici */
  if (end_addr > X86_64_MAX_PHYSICAL_ADDR) {
    klog_warn("x86_64: regione oltre limite fisico: end=0x%llx max=0x%llx", (unsigned long long)end_addr, (unsigned long long)X86_64_MAX_PHYSICAL_ADDR);
    return false;
  }

  /* allineamenti (warning non fatali) */
  if ((region->base % PAGE_SIZE) != 0) {
    klog_warn("x86_64: base non allineata a pagina: 0x%llx", (unsigned long long)region->base);
  }
  if (region->type == MEMORY_USABLE && region->length < PAGE_SIZE) {
    klog_debug("x86_64: USABLE < 1 pagina: %llu", (unsigned long long)region->length);
  }

  /* conflitti con MMIO/ACPI noti */
  for (size_t i = 0; i < MMIO_RANGE_COUNT; i++) {
    if (ranges_overlap(region->base, end_addr, mmio_ranges[i].base, mmio_ranges[i].end)) {
      klog_warn("x86_64: overlap MMIO 0x%llx-0x%llx con 0x%llx-0x%llx", (unsigned long long)region->base, (unsigned long long)end_addr, (unsigned long long)mmio_ranges[i].base,
                (unsigned long long)mmio_ranges[i].end);
      return false;
    }
  }
  for (size_t i = 0; i < ACPI_RANGE_COUNT; i++) {
    if (ranges_overlap(region->base, end_addr, acpi_reserved_ranges[i].base, acpi_reserved_ranges[i].end)) {
      klog_warn("x86_64: overlap ACPI 0x%llx-0x%llx con 0x%llx-0x%llx", (unsigned long long)region->base, (unsigned long long)end_addr,
                (unsigned long long)acpi_reserved_ranges[i].base, (unsigned long long)acpi_reserved_ranges[i].end);
      return false;
    }
  }

  return true;
}

/* ============================================================================
 * IMPLEMENTAZIONE API arch/memory.h
 * ==========================================================================*/

void arch_memory_init(void) {
  klog_info("x86_64: init memoria");

  if (!memmap_request.response) {
    klog_panic("x86_64: Limine memory map assente");
  }

  klog_info("x86_64: arch=%s", arch_get_name());
  klog_info("x86_64: page size=%zu bytes", (size_t)PAGE_SIZE);
  klog_info("x86_64: phys addr max=%u bit (max=0x%llx)", X86_64_MAX_PHYSICAL_BITS, (unsigned long long)X86_64_MAX_PHYSICAL_ADDR);

  struct limine_memmap_response *resp = memmap_request.response;
  klog_info("x86_64: Limine entries=%llu", (unsigned long long)resp->entry_count);

  if (resp->entry_count > (u64)KCFG_MAX_MEM_REGIONS) {
    klog_warn("x86_64: molte regioni (%llu) > cap=%d", (unsigned long long)resp->entry_count, KCFG_MAX_MEM_REGIONS);
  }

  /* CPUID checks */
  u32 eax = 0, ebx = 0, ecx = 0, edx = 0;
  cpu_cpuid(1, 0, &eax, &ebx, &ecx, &edx);

  if (!(edx & (1u << 6))) {
    klog_panic("x86_64: PAE non supportato");
  } else {
    klog_debug("x86_64: PAE ok");
  }

  if (!(edx & (1u << 12))) {
    klog_warn("x86_64: MTRR assente");
  } else {
    klog_debug("x86_64: MTRR ok");
  }

  if (!cpu_has_nx()) {
    klog_warn("x86_64: NX assente");
  } else {
    klog_debug("x86_64: NX ok");
  }

  cpu_cpuid(0x80000008u, 0, &eax, &ebx, &ecx, &edx);
  const u32 phys_bits = eax & 0xffu;
  klog_info("x86_64: cpu phys bits=%u", phys_bits);
  if (phys_bits > X86_64_MAX_PHYSICAL_BITS) {
    klog_warn("x86_64: phys bits=%u > gestito=%u", phys_bits, X86_64_MAX_PHYSICAL_BITS);
  }
}

size_t arch_memory_detect_regions(memory_region_t *regions, size_t max_regions) {
  if (!regions || max_regions == 0) {
    klog_error("x86_64: detect_regions param non valido");
    return -EINVAL;
  }
  if (!memmap_request.response) {
    klog_error("x86_64: Limine response assente");
    return -ENODEV;
  }

  struct limine_memmap_response *response = memmap_request.response;
  if (response->entry_count == 0) {
    klog_error("x86_64: Limine memmap vuota");
    return -ENOENT;
  }

  /* Hard cap al minimo tra sorgente, buffer chiamante e policy kernel. */
  u64 total_entries = response->entry_count;
  if (total_entries > (u64)max_regions)
    total_entries = (u64)max_regions;
  if (total_entries > (u64)KCFG_MAX_MEM_REGIONS)
    total_entries = (u64)KCFG_MAX_MEM_REGIONS;

  if (response->entry_count > total_entries) {
    klog_warn("x86_64: limito regioni da %llu a %llu", (unsigned long long)response->entry_count, (unsigned long long)total_entries);
  }

  memset(&cached_stats, 0, sizeof(cached_stats));
  size_t regions_count = 0;

  klog_info("x86_64: scansione %llu regioni Limine", (unsigned long long)total_entries);

  for (u64 i = 0; i < total_entries; i++) {
    struct limine_memmap_entry *le = response->entries[i];
    if (!le) {
      klog_warn("x86_64: entry Limine %llu NULL", (unsigned long long)i);
      continue;
    }

    memory_region_t *r = &regions[regions_count];
    r->base = le->base;
    r->length = le->length;
    r->type = convert_limine_type(le->type);

    if (!validate_memory_region(r)) {
      klog_warn("x86_64: scarto regione %llu [0x%llx-0x%llx] (%s)", (unsigned long long)i, (unsigned long long)r->base, (unsigned long long)(r->base + r->length - 1ull),
                memory_type_name(r->type));
      continue;
    }

    /* Statistiche */
    cached_stats.total_memory += r->length;
    switch (r->type) {
    case MEMORY_USABLE:
    case MEMORY_BOOTLOADER_RECLAIMABLE:
    case MEMORY_ACPI_RECLAIMABLE:
      cached_stats.usable_memory += r->length;
      if (r->length > cached_stats.largest_free_region) {
        cached_stats.largest_free_region = r->length;
      }
      break;
    case MEMORY_EXECUTABLE_AND_MODULES:
      cached_stats.executable_memory += r->length;
      break;
    case MEMORY_RESERVED:
    case MEMORY_ACPI_NVS:
    case MEMORY_BAD:
    case MEMORY_FRAMEBUFFER:
    default:
      cached_stats.reserved_memory += r->length;
      break;
    }

    /* Protezione sul buffer di destinazione. */
    if (regions_count + 1 >= max_regions) {
      regions_count++;
      klog_warn("x86_64: raggiunto limite buffer destinazione (%zu)", max_regions);
      break;
    }
    regions_count++;
  }

  stats_valid = true;

  klog_info("x86_64: regioni valide=%zu su src=%llu", regions_count, (unsigned long long)response->entry_count);
  klog_info("x86_64: tot=%llu MB, usable=%llu MB", (unsigned long long)(cached_stats.total_memory / MB), (unsigned long long)(cached_stats.usable_memory / MB));

  if (cached_stats.usable_memory < (u64)MIN_USABLE_MEMORY_MB * MB) {
    klog_error("x86_64: memoria usable insufficiente: %llu MB (min %u MB)", (unsigned long long)(cached_stats.usable_memory / MB), MIN_USABLE_MEMORY_MB);
  }

  return (size_t)regions_count;
}

bool arch_memory_region_valid(u64 base, u64 length) {
  if (length == 0)
    return false;
  if (base + length < base)
    return false;

  const u64 end_addr = base + length - 1ull;
  if (end_addr > X86_64_MAX_PHYSICAL_ADDR)
    return false;

  /* Pagina 0: warning informativo */
  if (base < X86_64_RESERVED_PAGE_0 && (base + length) > 0) {
    klog_debug("x86_64: regione include pagina 0: base=0x%llx", (unsigned long long)base);
  }

  if ((base % PAGE_SIZE) != 0) {
    klog_debug("x86_64: base non allineata a pagina: 0x%llx", (unsigned long long)base);
  }
  if ((length % PAGE_SIZE) != 0) {
    klog_debug("x86_64: len non multipla di pagina: %llu", (unsigned long long)length);
  }

  for (size_t i = 0; i < MMIO_RANGE_COUNT; i++) {
    if (ranges_overlap(base, end_addr, mmio_ranges[i].base, mmio_ranges[i].end)) {
      return false;
    }
  }
  for (size_t i = 0; i < ACPI_RANGE_COUNT; i++) {
    if (ranges_overlap(base, end_addr, acpi_reserved_ranges[i].base, acpi_reserved_ranges[i].end)) {
      return false;
    }
  }

  return true;
}

void arch_memory_get_stats(memory_stats_t *stats) {
  if (!stats) {
    klog_error("x86_64: get_stats() arg NULL");
    return;
  }
  if (!stats_valid) {
    klog_warn("x86_64: stats non valide (detect_regions non invocato?)");
    memset(stats, 0, sizeof(*stats));
    return;
  }
  memcpy(stats, &cached_stats, sizeof(*stats));
}
