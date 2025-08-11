/**
 * @file arch/x86_64/memory/memory.c
 * @brief Memory architecture layer implementation (x86_64) for ZONE-OS
 *
 * Implementa <arch/memory.h> usando la Limine memmap. Evitiamo blacklist/duplicazioni:
 * ci fidiamo della classificazione del bootloader (UEFI/e820→LIMINE_MEMMAP_*).
 * Manteniamo solo normalizzazione a pagina, coalescing e sanity CPU (non delegabile).
 *
 * Funzionalità:
 *  - arch_memory_init(): verifica memmap e requisiti CPU minimi (CPUID/PAE/NX/bits fisici)
 *  - arch_memory_detect_regions(): enumera, allinea/trunca a pagina, ordina e coalesca
 *  - arch_memory_get_stats(): totale/usable in byte
 *  - arch_memory_page_size(): 4096
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include <arch/cpu.h>
#include <arch/memory.h>
#include <arch/x86_64/cpu/cpu_lowlevel.h>
#include <klib/klog/klog.h>
#include <lib/stdint.h>
#include <lib/string/string.h>
#include <limine.h>
#include <mm/page.h>

#define X86_MAX_PHYS_BITS 52u
#define X86_MAX_PHYS_ADDR ((1ull << X86_MAX_PHYS_BITS) - 1ull)
#define NULL_GUARD_MIN 0x1000ull /* proteggi pagina 0 */

/* Limine: memory map request */
static volatile struct limine_memmap_request g_memmap_req = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

/* Statistiche cache */
static uint64_t g_total_bytes = 0;
static uint64_t g_usable_bytes = 0;
static int g_stats_valid = 0;

/* --- Helpers --- */
static inline arch_mem_type_t map_limine_type(uint64_t t) {
  switch (t) {
  case LIMINE_MEMMAP_USABLE:
    return ARCH_MEM_USABLE;
  case LIMINE_MEMMAP_RESERVED:
    return ARCH_MEM_RESERVED;
  case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
    return ARCH_MEM_ACPI_RECLAIM;
  case LIMINE_MEMMAP_ACPI_NVS:
    return ARCH_MEM_ACPI_NVS;
  case LIMINE_MEMMAP_BAD_MEMORY:
    return ARCH_MEM_BAD;
  case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
    return ARCH_MEM_BOOT_RECLAIM;
  case LIMINE_MEMMAP_KERNEL_AND_MODULES:
    return ARCH_MEM_KERNEL;
  case LIMINE_MEMMAP_FRAMEBUFFER:
    return ARCH_MEM_FRAMEBUFFER;
  default:
    return ARCH_MEM_RESERVED;
  }
}

/* Allinea [base,end] a pagina e taglia la pagina 0; rispetta limiti fisici. */
static int normalize_region(arch_mem_region_t *r) {
  if (!r || r->length == 0)
    return 0;
  if (r->base + r->length < r->base)
    return 0; /* overflow */

  uint64_t lo = (r->base < NULL_GUARD_MIN) ? NULL_GUARD_MIN : r->base;
  uint64_t hi = r->base + r->length - 1;

  uint64_t new_lo = mm_page_align_up(lo);
  uint64_t new_hi = mm_page_align_down(hi + 1) - 1;
  if (new_hi < new_lo)
    return 0;
  if (new_hi > X86_MAX_PHYS_ADDR)
    return 0;

  r->base = new_lo;
  r->length = (new_hi - new_lo + 1);
  return r->length != 0;
}

/* insertion sort per n ridotti (tipico) */
static void sort_by_base(arch_mem_region_t *a, size_t n) {
  for (size_t i = 1; i < n; ++i) {
    arch_mem_region_t k = a[i];
    size_t j = i;
    while (j && a[j - 1].base > k.base) {
      a[j] = a[j - 1];
      --j;
    }
    a[j] = k;
  }
}

/* merge contigui dello stesso tipo */
static size_t coalesce_adjacent(arch_mem_region_t *a, size_t n) {
  if (!n)
    return 0;
  size_t w = 0;
  for (size_t i = 1; i < n; ++i) {
    uint64_t end = a[w].base + a[w].length;
    if (a[i].type == a[w].type && a[i].base == end)
      a[w].length += a[i].length;
    else
      a[++w] = a[i];
  }
  return w + 1;
}

/* --- Sanity CPU (non duplicato dal bootloader) --- */
static int cpu_memory_sanity(void) {
  /* Su x86_64 CPUID è garantita; usiamo direttamente cpu_cpuid() */
  uint32_t a = 0, b = 0, c = 0, d = 0;

  /* CPUID.1: PAE obbligatorio (EDX[6]) */
  cpu_cpuid(1, 0, &a, &b, &c, &d);
  if ((d & (1u << 6)) == 0) {
    klog_panic("x86_64/memory: CPU senza PAE");
    return 0;
  }

  /* CPUID.80000001h: NX informativo (EDX[20]) */
  cpu_cpuid(0x80000001u, 0, &a, &b, &c, &d);
  if ((d & (1u << 20)) == 0)
    klog_warn("x86_64/memory: NX non supportato");

  /* CPUID.80000008h: numero di bit fisici (EAX[7:0]) — solo logging */
  cpu_cpuid(0x80000008u, 0, &a, &b, &c, &d);
  {
    uint32_t phys_bits = a & 0xFFu;
    if (!phys_bits || phys_bits > X86_MAX_PHYS_BITS)
      klog_warn("x86_64/memory: phys bits anomali: %u", phys_bits);
  }

  /* Se vuoi loggare EFER.NXE attivo (non blocca il boot): */
  if (cpu_has_msr()) {
    const uint64_t EFER = 0xC0000080u;
    const uint64_t efer = cpu_rdmsr((uint32_t)EFER);
    if ((efer & (1ull << 11)) == 0)
      klog_warn("x86_64/memory: EFER.NXE non attivo");
  }
  return 1;
}

/* --- API --- */
void arch_memory_init(void) {
  if (!g_memmap_req.response)
    klog_panic("x86_64/memory: Limine memmap non disponibile");
  if (!cpu_memory_sanity())
    klog_panic("x86_64/memory: requisiti CPU non soddisfatti");
}

size_t arch_memory_detect_regions(arch_mem_region_t *out, size_t max) {
  if (!out || !max) {
    g_stats_valid = 0;
    return 0;
  }
  if (!g_memmap_req.response) {
    g_stats_valid = 0;
    return 0;
  }

  struct limine_memmap_response *r = g_memmap_req.response;
  const uint64_t limit = (r->entry_count < (uint64_t)max) ? r->entry_count : (uint64_t)max;

  size_t n = 0;
  g_total_bytes = g_usable_bytes = 0;

  for (uint64_t i = 0; i < limit; ++i) {
    const struct limine_memmap_entry *e = r->entries[i];
    if (!e || !e->length)
      continue;

    arch_mem_region_t reg = {.base = e->base, .length = e->length, .type = map_limine_type(e->type)};
    if (!normalize_region(&reg))
      continue;

    out[n++] = reg;

    g_total_bytes += reg.length;
    switch (reg.type) {
    case ARCH_MEM_USABLE:
    case ARCH_MEM_BOOT_RECLAIM:
    case ARCH_MEM_ACPI_RECLAIM:
      g_usable_bytes += reg.length;
      break;
    default:
      break;
    }
  }

  sort_by_base(out, n);
  n = coalesce_adjacent(out, n);

  g_stats_valid = 1;
  return n;
}

void arch_memory_get_stats(uint64_t *total, uint64_t *usable) {
  if (total)
    *total = g_stats_valid ? g_total_bytes : 0;
  if (usable)
    *usable = g_stats_valid ? g_usable_bytes : 0;
}

uint64_t arch_memory_page_size(void) {
  return mm_page_size();
}
