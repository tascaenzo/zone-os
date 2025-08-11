/**
 * @file mm/pmm.c
 * @brief Physical Memory Manager (arch-indipendente) per ZONE-OS
 *
 * Gestione memoria fisica a pagine tramite bitmap. Dipende da <arch/memory.h>
 * per discovery della mappa fisica e per la dimensione pagina.
 *
 * Autore: Enzo Tasca
 * @date 2025
 */

#include <arch/memory.h>
#include <klib/klog/klog.h>
#include <klib/spinlock/spinlock.h>
#include <lib/string/string.h>
#include <lib/types.h>
#include <mm/pmm.h>

/* -------------------------------------------------------------------------- */
/*                                Costanti                                    */
/* -------------------------------------------------------------------------- */

#ifndef ARCH_MAX_MEMORY_REGIONS
#define ARCH_MAX_MEMORY_REGIONS 512
#endif

#ifndef KB
#define KB 1024ULL
#endif
#ifndef MB
#define MB (1024ULL * KB)
#endif

/* -------------------------------------------------------------------------- */
/*                              Stato interno                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Stato del PMM.
 */
typedef struct {
  bool initialized;        /**< PMM pronto all'uso */
  u8 *bitmap;              /**< Bitmap pagine (1 bit per pagina) */
  u64 bitmap_size;         /**< Dimensione bitmap in byte */
  u64 total_pages;         /**< Pagine gestite */
  u64 next_free_hint;      /**< Hint per ricerca next-fit */
  u64 page_size;           /**< Dimensione pagina (HAL) */
  u64 total_memory_bytes;  /**< Memoria fisica totale */
  u64 usable_memory_bytes; /**< Memoria fisica utilizzabile */
} pmm_state_t;

static pmm_state_t pmm_state = {.initialized = false};
static pmm_stats_t pmm_stats;
static spinlock_t pmm_lock = SPINLOCK_INITIALIZER;

/* -------------------------------------------------------------------------- */
/*                           Helper generici pagina                            */
/* -------------------------------------------------------------------------- */

static inline u64 PAGE_SIZE_LOCAL(void) {
  return pmm_state.page_size;
}
static inline u64 PAGE_ALIGN_UP_LOCAL(u64 x) {
  u64 ps = PAGE_SIZE_LOCAL();
  return (x + ps - 1) / ps * ps;
}
static inline u64 PAGE_ALIGN_DOWN_LOCAL(u64 x) {
  u64 ps = PAGE_SIZE_LOCAL();
  return x / ps * ps;
}
static inline u64 ADDR_TO_PAGE_LOCAL(u64 a) {
  return a / PAGE_SIZE_LOCAL();
}
static inline u64 PAGE_TO_ADDR_LOCAL(u64 p) {
  return p * PAGE_SIZE_LOCAL();
}

/* -------------------------------------------------------------------------- */
/*                              Bitmap helpers                                 */
/* -------------------------------------------------------------------------- */

#define BITMAP_SET_BIT(b, bit) ((b)[(bit) / 8] |= (u8)(1u << ((bit) % 8)))
#define BITMAP_CLEAR_BIT(b, bit) ((b)[(bit) / 8] &= (u8) ~(1u << ((bit) % 8)))
#define BITMAP_TEST_BIT(b, bit) ((b)[(bit) / 8] & (u8)(1u << ((bit) % 8)))

static inline void pmm_update_hint_locked(u64 new_hint) {
  pmm_state.next_free_hint = (new_hint < pmm_state.total_pages) ? new_hint : 0;
}

/* Stato pagina */
static void pmm_mark_page_used(u64 page_index) {
  if (page_index < pmm_state.total_pages)
    BITMAP_SET_BIT(pmm_state.bitmap, page_index);
}
static void pmm_mark_page_free(u64 page_index) {
  if (page_index < pmm_state.total_pages)
    BITMAP_CLEAR_BIT(pmm_state.bitmap, page_index);
}
static bool pmm_is_page_used_internal(u64 page_index) {
  if (page_index >= pmm_state.total_pages)
    return true;
  return (BITMAP_TEST_BIT(pmm_state.bitmap, page_index) != 0);
}

/* -------------------------------------------------------------------------- */
/*                              Ricostruzione stats                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Ricostruisce le statistiche scorrendo il bitmap.
 */
static void pmm_update_stats(void) {
  u64 free_count = 0, used_count = 0;
  for (u64 i = 0; i < pmm_state.total_pages; ++i) {
    if (pmm_is_page_used_internal(i))
      used_count++;
    else
      free_count++;
  }
  pmm_stats.total_pages = pmm_state.total_pages;
  pmm_stats.free_pages = free_count;
  pmm_stats.used_pages = used_count;
}

/* -------------------------------------------------------------------------- */
/*                               Ricerca pagine                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Trova la prima pagina libera a partire da un hint.
 */
static u64 pmm_find_free_page_from(u64 start_hint) {
  for (u64 i = start_hint; i < pmm_state.total_pages; ++i)
    if (!pmm_is_page_used_internal(i))
      return i;
  for (u64 i = 0; i < start_hint; ++i)
    if (!pmm_is_page_used_internal(i))
      return i;
  return pmm_state.total_pages;
}

/**
 * @brief Trova un blocco contiguo di @p count pagine libere.
 */
static u64 pmm_find_free_pages_from(u64 start_hint, size_t count) {
  for (u64 start = start_hint; start + count <= pmm_state.total_pages; ++start) {
    bool found = true;
    for (size_t i = 0; i < count; ++i) {
      if (pmm_is_page_used_internal(start + i)) {
        found = false;
        start += i;
        break;
      }
    }
    if (found)
      return start;
  }
  for (u64 start = 0; start < start_hint && start + count <= pmm_state.total_pages; ++start) {
    bool found = true;
    for (size_t i = 0; i < count; ++i) {
      if (pmm_is_page_used_internal(start + i)) {
        found = false;
        start += i;
        break;
      }
    }
    if (found)
      return start;
  }
  return pmm_state.total_pages;
}

/* -------------------------------------------------------------------------- */
/*                                  API PMM                                    */
/* -------------------------------------------------------------------------- */

pmm_result_t pmm_init(void) {
  klog_info("PMM: init");

  /* HAL */
  arch_memory_init();
  pmm_state.page_size = arch_memory_page_size();
  if (!pmm_state.page_size)
    return PMM_NOT_INITIALIZED;

  /* Discovery mappa fisica (HAL) */
  arch_mem_region_t areg[ARCH_MAX_MEMORY_REGIONS];
  size_t acnt = arch_memory_detect_regions(areg, ARCH_MAX_MEMORY_REGIONS);
  if (acnt == 0) {
    klog_error("PMM: memmap vuota");
    return PMM_NOT_INITIALIZED;
  }

  /* Limiti e sommatorie */
  u64 highest_addr = 0, total_bytes = 0, usable_bytes = 0;
  for (size_t i = 0; i < acnt; ++i) {
    const arch_mem_region_t *r = &areg[i];
    total_bytes += r->length;
    const u64 end = r->base + r->length;
    if (end > highest_addr)
      highest_addr = end;
    if (r->type == ARCH_MEM_USABLE || r->type == ARCH_MEM_BOOT_RECLAIM || r->type == ARCH_MEM_ACPI_RECLAIM)
      usable_bytes += r->length;
  }

  pmm_state.total_pages = ADDR_TO_PAGE_LOCAL(highest_addr);
  pmm_state.total_memory_bytes = total_bytes;
  pmm_state.usable_memory_bytes = usable_bytes;

  /* Bitmap */
  pmm_state.bitmap_size = (pmm_state.total_pages + 7) / 8;
  const u64 bitmap_pages_needed = PAGE_ALIGN_UP_LOCAL(pmm_state.bitmap_size) / PAGE_SIZE_LOCAL();

  /* Posizionamento bitmap in USABLE */
  u64 bitmap_addr = 0;
  bool bitmap_found = false;
  for (size_t i = 0; i < acnt; ++i) {
    const arch_mem_region_t *r = &areg[i];
    if (r->type != ARCH_MEM_USABLE)
      continue;
    const u64 ab = PAGE_ALIGN_UP_LOCAL(r->base);
    const u64 avail = (r->base + r->length) - ab;
    if (avail >= pmm_state.bitmap_size) {
      bitmap_addr = ab;
      bitmap_found = true;
      break;
    }
  }
  if (!bitmap_found) {
    klog_error("PMM: spazio per bitmap non trovato");
    return PMM_OUT_OF_MEMORY;
  }

  pmm_state.bitmap = (u8 *)bitmap_addr;
  pmm_stats.bitmap_pages = bitmap_pages_needed;

  /* Inizializza bitmap: tutto occupato */
  memset(pmm_state.bitmap, 0xFF, pmm_state.bitmap_size);

  /* Libera regioni USABLE/RECLAIM */
  for (size_t i = 0; i < acnt; ++i) {
    const arch_mem_region_t *r = &areg[i];
    const u64 start = PAGE_ALIGN_UP_LOCAL(r->base);
    const u64 end = PAGE_ALIGN_DOWN_LOCAL(r->base + r->length);
    if (end <= start)
      continue;

    const u64 sp = ADDR_TO_PAGE_LOCAL(start);
    const u64 ep = ADDR_TO_PAGE_LOCAL(end) - 1;

    switch (r->type) {
    case ARCH_MEM_USABLE:
    case ARCH_MEM_BOOT_RECLAIM:
    case ARCH_MEM_ACPI_RECLAIM:
      for (u64 p = sp; p <= ep; ++p)
        pmm_mark_page_free(p);
      break;
    default:
      pmm_stats.reserved_pages += (ep - sp + 1);
      break;
    }
  }

  /* Protezione bitmap */
  const u64 bmp_sp = ADDR_TO_PAGE_LOCAL(bitmap_addr);
  const u64 bmp_ep = ADDR_TO_PAGE_LOCAL(bitmap_addr + pmm_state.bitmap_size - 1);
  for (u64 p = bmp_sp; p <= bmp_ep; ++p)
    pmm_mark_page_used(p);

  /* Protezione pagina 0 */
  pmm_mark_page_used(0);

  /* Statistiche e stato */
  pmm_update_stats();
  pmm_update_hint_locked(0);
  pmm_state.initialized = true;

  klog_info("PMM: ok — total=%lu MB usable=%lu MB pages=%lu", pmm_state.total_memory_bytes / MB, pmm_state.usable_memory_bytes / MB, pmm_state.total_pages);
  return PMM_SUCCESS;
}

void *pmm_alloc_page(void) {
  if (!pmm_state.initialized)
    return NULL;

  spinlock_lock(&pmm_lock);
  if (pmm_stats.free_pages == 0) {
    spinlock_unlock(&pmm_lock);
    return NULL;
  }

  const u64 idx = pmm_find_free_page_from(pmm_state.next_free_hint);
  if (idx >= pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return NULL;
  }

  pmm_mark_page_used(idx);
  pmm_stats.free_pages--;
  pmm_stats.used_pages++;
  pmm_stats.alloc_count++;
  pmm_update_hint_locked(idx + 1);

  spinlock_unlock(&pmm_lock);
  return (void *)PAGE_TO_ADDR_LOCAL(idx);
}

void *pmm_alloc_pages(size_t count) {
  if (!pmm_state.initialized || count == 0)
    return NULL;

  spinlock_lock(&pmm_lock);
  if (pmm_stats.free_pages < count) {
    spinlock_unlock(&pmm_lock);
    return NULL;
  }

  const u64 start = pmm_find_free_pages_from(pmm_state.next_free_hint, count);
  if (start >= pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return NULL;
  }

  for (size_t i = 0; i < count; ++i)
    pmm_mark_page_used(start + i);
  pmm_stats.free_pages -= count;
  pmm_stats.used_pages += count;
  pmm_stats.alloc_count++;
  pmm_update_hint_locked(start + count);

  spinlock_unlock(&pmm_lock);
  return (void *)PAGE_TO_ADDR_LOCAL(start);
}

pmm_result_t pmm_free_page(void *page) {
  if (!pmm_state.initialized || !page)
    return PMM_NOT_INITIALIZED;

  const u64 addr = (u64)page;
  if (addr % PAGE_SIZE_LOCAL() != 0)
    return PMM_INVALID_ADDRESS;

  const u64 idx = ADDR_TO_PAGE_LOCAL(addr);

  spinlock_lock(&pmm_lock);
  if (idx >= pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return PMM_INVALID_ADDRESS;
  }
  if (!pmm_is_page_used_internal(idx)) {
    spinlock_unlock(&pmm_lock);
    return PMM_ALREADY_FREE;
  }

  pmm_mark_page_free(idx);
  pmm_stats.free_pages++;
  pmm_stats.used_pages--;
  pmm_stats.free_count++;
  if (idx < pmm_state.next_free_hint)
    pmm_update_hint_locked(idx);

  spinlock_unlock(&pmm_lock);
  return PMM_SUCCESS;
}

pmm_result_t pmm_free_pages(void *pages, size_t count) {
  if (!pmm_state.initialized || !pages || count == 0)
    return PMM_NOT_INITIALIZED;

  const u64 addr = (u64)pages;
  if (addr % PAGE_SIZE_LOCAL() != 0)
    return PMM_INVALID_ADDRESS;

  const u64 start = ADDR_TO_PAGE_LOCAL(addr);

  spinlock_lock(&pmm_lock);

  if (start + count > pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return PMM_INVALID_ADDRESS;
  }

  for (size_t i = 0; i < count; ++i)
    if (!pmm_is_page_used_internal(start + i)) {
      spinlock_unlock(&pmm_lock);
      return PMM_ALREADY_FREE;
    }

  for (size_t i = 0; i < count; ++i)
    pmm_mark_page_free(start + i);
  pmm_stats.free_pages += count;
  pmm_stats.used_pages -= count;
  pmm_stats.free_count++;
  if (start < pmm_state.next_free_hint)
    pmm_update_hint_locked(start);

  spinlock_unlock(&pmm_lock);
  return PMM_SUCCESS;
}

bool pmm_is_page_free(void *page) {
  if (!pmm_state.initialized || !page)
    return false;
  const u64 addr = (u64)page;
  if (addr % PAGE_SIZE_LOCAL() != 0)
    return false;
  const u64 idx = ADDR_TO_PAGE_LOCAL(addr);
  if (idx >= pmm_state.total_pages)
    return false;
  return !pmm_is_page_used_internal(idx);
}

const pmm_stats_t *pmm_get_stats(void) {
  if (!pmm_state.initialized)
    return NULL;
  return &pmm_stats;
}

bool pmm_get_page_info(void *page, u64 *page_index, bool *is_free) {
  if (!pmm_state.initialized || !page)
    return false;
  const u64 addr = (u64)page;
  if (addr % PAGE_SIZE_LOCAL() != 0)
    return false;
  const u64 idx = ADDR_TO_PAGE_LOCAL(addr);
  if (idx >= pmm_state.total_pages)
    return false;
  if (page_index)
    *page_index = idx;
  if (is_free)
    *is_free = !pmm_is_page_used_internal(idx);
  return true;
}

void pmm_print_info(void) {
  if (!pmm_state.initialized) {
    klog_error("PMM: non inizializzato");
    return;
  }
  klog_info("PMM: total=%lu MB usable=%lu MB", pmm_state.total_memory_bytes / MB, pmm_state.usable_memory_bytes / MB);
  klog_info("PMM: pages total=%lu free=%lu used=%lu reserved=%lu", pmm_stats.total_pages, pmm_stats.free_pages, pmm_stats.used_pages, pmm_stats.reserved_pages);
  klog_info("PMM: bitmap=%lu bytes (%lu pages) page_size=%lu", pmm_state.bitmap_size, pmm_stats.bitmap_pages, PAGE_SIZE_LOCAL());
}

bool pmm_check_integrity(void) {
  if (!pmm_state.initialized)
    return false;
  u64 free_count = 0, used_count = 0;
  for (u64 i = 0; i < pmm_state.total_pages; ++i)
    if (pmm_is_page_used_internal(i))
      used_count++;
    else
      free_count++;
  return (free_count == pmm_stats.free_pages) && (used_count == pmm_stats.used_pages);
}

bool pmm_validate_stats(void) {
  const u64 accounted = pmm_stats.free_pages + pmm_stats.used_pages;
  if (accounted > pmm_stats.total_pages)
    return false;
  if (pmm_stats.largest_free_run > pmm_stats.free_pages)
    return false;
  return true;
}

size_t pmm_find_largest_free_run(size_t *start_page) {
  if (!pmm_state.initialized)
    return 0;

  size_t max_run = 0, cur_run = 0, max_start = 0, cur_start = 0;
  for (u64 i = 0; i < pmm_state.total_pages; ++i) {
    if (!pmm_is_page_used_internal(i)) {
      if (cur_run == 0)
        cur_start = (size_t)i;
      cur_run++;
      if (cur_run > max_run) {
        max_run = cur_run;
        max_start = cur_start;
      }
    } else {
      cur_run = 0;
    }
  }
  if (start_page)
    *start_page = max_start;
  pmm_stats.largest_free_run = max_run;
  return max_run;
}

void pmm_print_fragmentation_info(void) {
  if (!pmm_state.initialized)
    return;
  size_t start = 0;
  size_t run = pmm_find_largest_free_run(&start);
  klog_info("PMM: largest free run = %lu pages (%lu MB) @page %lu (0x%lx)", run, (run * PAGE_SIZE_LOCAL()) / MB, start, PAGE_TO_ADDR_LOCAL(start));
  if (pmm_stats.free_pages) {
    u64 frag = 100 - ((run * 100) / pmm_stats.free_pages);
    klog_info("PMM: fragmentation = %lu%%", frag);
  }
}

u64 pmm_get_page_size(void) {
  return PAGE_SIZE_LOCAL();
}
