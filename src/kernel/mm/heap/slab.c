#include "slab.h"
#include "heap.h"
#include <klib/klog/klog.h>
#include <klib/list/list.h>
#include <lib/stdio/stdio.h>
#include <lib/string/string.h>
#include <mm/page.h>
#include <mm/pmm.h>

#define list_first_entry(head, type, member) LIST_ENTRY((head)->next, type, member)
#define list_insert_tail(head, node) list_insert_before((head), (node))

// Esposti anche per altri moduli (heap.c)
slab_cache_t slab_caches[SLAB_MAX_CACHES];
u32 slab_cache_count = 0;

void slab_init(void) {
  memset(slab_caches, 0, sizeof(slab_caches));

  size_t size = SLAB_MIN_SIZE;
  while (size <= SLAB_MAX_SIZE && slab_cache_count < SLAB_MAX_CACHES) {
    char name[32];
    ksnprintf(name, sizeof(name), "slab_%zu", size);
    slab_cache_t *cache = slab_cache_create(name, size, 8, NULL, NULL);
    if (!cache) {
      klog_warn("slab: creazione cache fallita per size %zu", size);
      break;
    }
    size <<= 1;
  }
}

slab_cache_t *slab_find_cache_for_size(size_t size) {
  for (u32 i = 0; i < slab_cache_count; ++i) {
    if (slab_caches[i].object_size >= size) {
      return &slab_caches[i];
    }
  }
  return (slab_cache_t *)NULL;
}

void *slab_alloc(size_t size) {
  slab_cache_t *cache = slab_find_cache_for_size(size);
  if (!cache)
    return NULL;
  return slab_cache_alloc(cache);
}

void slab_free(void *ptr) {
  slab_cache_t *cache = slab_find_cache_for_ptr(ptr);
  if (!cache)
    return;
  slab_cache_free(cache, ptr);
}

slab_cache_t *slab_find_cache_for_ptr(void *ptr) {
  for (u32 i = 0; i < slab_cache_count; ++i) {
    slab_cache_t *cache = &slab_caches[i];
    spinlock_lock(&cache->lock);
    list_node_t *lists[] = {&cache->full_slabs, &cache->partial_slabs, &cache->empty_slabs};
    for (int l = 0; l < 3; ++l) {
      list_node_t *it;
      LIST_FOR_EACH(it, lists[l]) {
        slab_t *slab = LIST_ENTRY(it, slab_t, node);
        u8 *base = (u8 *)slab->page_addr;
        u8 *end = base + arch_memory_page_size();
        if ((u8 *)ptr >= base && (u8 *)ptr < end) {
          spinlock_unlock(&cache->lock);
          return cache;
        }
      }
    }
    spinlock_unlock(&cache->lock);
  }
  return (slab_cache_t *)NULL;
}

slab_cache_t *slab_cache_create(const char *name, size_t object_size, size_t align, slab_ctor_t ctor, slab_dtor_t dtor) {
  if (slab_cache_count >= SLAB_MAX_CACHES || object_size == 0)
    return (slab_cache_t *)NULL;

  slab_cache_t *cache = &slab_caches[slab_cache_count++];
  memset(cache, 0, sizeof(*cache));

  strncpy(cache->name, name, sizeof(cache->name) - 1);
  cache->object_size = object_size;
  cache->align = align;
  cache->ctor = ctor;
  cache->dtor = dtor;
  cache->magic = SLAB_MAGIC_CACHE;
  list_init(&cache->full_slabs);
  list_init(&cache->partial_slabs);
  list_init(&cache->empty_slabs);
  spinlock_init(&cache->lock);
  return cache;
}

void *slab_cache_alloc(slab_cache_t *cache) {
  spinlock_lock(&cache->lock);

  slab_t *slab = (slab_t *)NULL;
  if (!list_is_empty(&cache->partial_slabs)) {
    slab = list_first_entry(&cache->partial_slabs, slab_t, node);
  } else if (!list_is_empty(&cache->empty_slabs)) {
    slab = list_first_entry(&cache->empty_slabs, slab_t, node);
    list_remove(&slab->node);
    list_insert_tail(&cache->partial_slabs, &slab->node);
  } else {
    void *page = pmm_alloc_page();
    if (!page) {
      spinlock_unlock(&cache->lock);
      return NULL;
    }

    slab = (slab_t *)page;
    memset(slab, 0, sizeof(slab_t));
    slab->page_addr = page;
    slab->object_size = cache->object_size;
    slab->magic = SLAB_MAGIC_ALLOC;

    size_t obj_count = SLAB_OBJECTS_PER_PAGE(cache->object_size);
    slab->total_objects = obj_count;
    slab->free_objects = obj_count;

    u8 *data = (u8 *)slab + sizeof(slab_t);
    for (size_t i = 0; i < obj_count; ++i) {
      slab_object_t *obj = (slab_object_t *)(data + i * cache->object_size);
      obj->next_free = slab->free_list;
      slab->free_list = obj;
    }

    list_insert_tail(&cache->partial_slabs, &slab->node);
    cache->total_slabs++;
  }

  slab_object_t *obj = slab->free_list;
  slab->free_list = obj->next_free;
  slab->free_objects--;
  cache->allocated_objects++;
  cache->alloc_count++;

  if (slab->free_objects == 0) {
    list_remove(&slab->node);
    list_insert_tail(&cache->full_slabs, &slab->node);
  }

  spinlock_unlock(&cache->lock);
  return (void *)obj;
}

void slab_cache_free(slab_cache_t *cache, void *ptr) {
  if (!cache || !ptr)
    return;

  spinlock_lock(&cache->lock);

  slab_t *slab = (slab_t *)mm_page_align_down((u64)ptr);
  if (slab->magic != SLAB_MAGIC_ALLOC) {
    spinlock_unlock(&cache->lock);
    return;
  }

  slab_object_t *obj = (slab_object_t *)ptr;
  obj->next_free = slab->free_list;
  slab->free_list = obj;
  slab->free_objects++;
  cache->allocated_objects--;
  cache->free_count++;

  list_remove(&slab->node);

  if (slab->free_objects == slab->total_objects) {
    list_insert_tail(&cache->empty_slabs, &slab->node);
  } else {
    list_insert_tail(&cache->partial_slabs, &slab->node);
  }

  spinlock_unlock(&cache->lock);
}

void slab_dump_caches(void) {
  for (u32 i = 0; i < slab_cache_count; ++i) {
    slab_cache_t *cache = &slab_caches[i];
    klog_info("slab: cache %s - allocati=%u, tot_slab=%u", cache->name, cache->allocated_objects, cache->total_slabs);
  }
}
bool slab_check_integrity(slab_cache_t *cache) {
  if (!cache || cache->magic != SLAB_MAGIC_CACHE)
    return false;

  list_node_t *it;
  list_node_t *lists[] = {&cache->full_slabs, &cache->partial_slabs, &cache->empty_slabs};

  for (int i = 0; i < 3; ++i) {
    LIST_FOR_EACH(it, lists[i]) {
      slab_t *slab = LIST_ENTRY(it, slab_t, node);
      if (!slab || slab->magic != SLAB_MAGIC_ALLOC) {
        klog_warn("slab_check_integrity: slab con magic non valido nella cache %s", cache->name);
        return false;
      }
    }
  }

  return true;
}