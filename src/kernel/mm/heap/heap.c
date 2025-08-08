#include "heap.h"
#include "buddy.h"
#include "slab.h"
#include <klib/bitmap/bitmap.h>
#include <klib/klog/klog.h>
#include <klib/list/list.h>
#include <lib/stdbool.h>
#include <lib/stdio/stdio.h>
#include <lib/string/string.h>
#include <mm/memory.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define HEAP_SLAB_MAX_SIZE 2048

static buddy_allocator_t buddy;
static u64 buddy_bitmap[(1 << 18) / 64];
static bool heap_initialized = false;

void heap_init(void) {
  u64 base = 0, size = 0;
  if (!memory_find_largest_region(&base, &size)) {
    klog_panic("heap: nessuna regione valida trovata");
    return;
  }

  base = PAGE_ALIGN_UP(base);
  size = PAGE_ALIGN_DOWN(size);

  if (!buddy_init(&buddy, base, size, buddy_bitmap, sizeof(buddy_bitmap) * 8)) {
    klog_panic("heap: inizializzazione buddy fallita");
    return;
  }

  slab_init();
  heap_initialized = true;

  klog_info("heap: inizializzato su [%p - %p] (%llu KB)", (void *)base, (void *)(base + size), size / 1024);
}

void *kmalloc(size_t size) {
  if (!heap_initialized || size == 0)
    return NULL;

  if (size <= HEAP_SLAB_MAX_SIZE) {
    klog_debug("kmalloc(%lu): SLAB allocator", size);
    return slab_alloc(size);
  }

  klog_debug("kmalloc(%lu): BUDDY allocator", size);
  u64 phys = buddy_alloc(&buddy, size);
  if (!phys)
    return NULL;

  return (void *)vmm_phys_to_virt(phys);
}

void *kcalloc(size_t nmemb, size_t size) {
  size_t total = nmemb * size;
  void *ptr = kmalloc(total);
  if (ptr)
    memset(ptr, 0, total);
  return ptr;
}

void kfree(void *ptr) {
  if (!heap_initialized || !ptr)
    return;

  if (SLAB_PTR_VALID(ptr)) {
    slab_cache_t *cache = slab_find_cache_for_ptr(ptr);
    if (cache) {
      slab_cache_free(cache, ptr);
      return;
    }
  }

  u64 phys = vmm_virt_to_phys((u64)ptr);
  buddy_free(&buddy, phys);
}

void heap_dump_info(void) {
  klog_info("heap: stato slab + buddy");
  slab_dump_caches();
  buddy_dump(&buddy);
}

bool heap_check_integrity(void) {
  bool slab_ok = true;
  for (u32 i = 0; i < slab_cache_count; ++i) {
    slab_cache_t *cache = &slab_caches[i];
    if (cache->magic != SLAB_MAGIC_CACHE)
      continue;
    if (!slab_check_integrity(cache)) {
      klog_warn("heap: slab [%s] corrotta", cache->name);
      slab_ok = false;
    }
  }
  return slab_ok && buddy_check_integrity(&buddy);
}
