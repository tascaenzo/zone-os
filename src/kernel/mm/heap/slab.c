#include <arch/x86_64/vmm_defs.h>
#include <klib/klog.h>
#include <klib/list.h>
#include <klib/spinlock.h>
#include <lib/string.h>
#include <mm/heap/slab.h>
#include <mm/pmm.h>

/*
 * Real basic slab allocator implementation.
 * Each cache manages a list of slabs (4KB pages) from which objects are
 * allocated. Objects are carved from the page after the slab_t header.
 */

#define SLAB_MAGIC 0x51AB51ABu

static slab_stats_t g_stats;
static slab_cache_t *g_caches[SLAB_MAX_CACHES];
static u32 g_cache_count = 0;

static const size_t default_sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
static const u32 default_size_count = sizeof(default_sizes) / sizeof(default_sizes[0]);

/* ----------------------------------------------------------- */
/*                      Helper Functions                       */
/* ----------------------------------------------------------- */

static void *alloc_page(void) {
    void *phys = pmm_alloc_page();
    if (!phys)
        return NULL;
    return VMM_X86_64_PHYS_TO_VIRT((u64)phys);
}

static void free_page(void *virt) {
    void *phys = (void *)VMM_X86_64_VIRT_TO_PHYS(virt);
    pmm_free_page(phys);
}

static slab_cache_t *find_best_cache(size_t size) {
    slab_cache_t *best = NULL;
    for (u32 i = 0; i < g_cache_count; i++) {
        slab_cache_t *c = g_caches[i];
        if (c->object_size >= size) {
            if (!best || c->object_size < best->object_size)
                best = c;
        }
    }
    return best;
}

static slab_t *create_slab(slab_cache_t *cache) {
    void *page = alloc_page();
    if (!page)
        return NULL;

    slab_t *slab = (slab_t *)page;
    list_init(&slab->node);
    slab->page_addr = (void *)VMM_X86_64_VIRT_TO_PHYS(page);
    slab->object_size = cache->object_size;
    slab->magic = SLAB_MAGIC;

    u16 objs = SLAB_OBJECTS_PER_PAGE(cache->object_size);
    slab->total_objects = objs;
    slab->free_objects = objs;
    slab->free_list = NULL;

    u8 *obj_area = (u8 *)page + sizeof(slab_t);
    for (u16 i = 0; i < objs; i++) {
        slab_object_t *obj = (slab_object_t *)(obj_area + i * cache->object_size);
        obj->next_free = slab->free_list;
        slab->free_list = obj;
    }

    cache->total_slabs++;
    cache->total_objects += objs;
    g_stats.total_slabs++;
    g_stats.total_memory += SLAB_PAGE_SIZE;
    g_stats.overhead_memory += sizeof(slab_t);

    return slab;
}

static void destroy_slab(slab_cache_t *cache, slab_t *slab) {
    free_page((void *)VMM_X86_64_PHYS_TO_VIRT((u64)slab->page_addr));
    cache->total_slabs--;
    if (cache->total_objects >= slab->total_objects)
        cache->total_objects -= slab->total_objects;
    g_stats.total_slabs--;
    if (g_stats.total_memory >= SLAB_PAGE_SIZE)
        g_stats.total_memory -= SLAB_PAGE_SIZE;
    if (g_stats.overhead_memory >= sizeof(slab_t))
        g_stats.overhead_memory -= sizeof(slab_t);
}

/* ----------------------------------------------------------- */
/*                        Core API                             */
/* ----------------------------------------------------------- */

void slab_init(void) {
    memset(&g_stats, 0, sizeof(g_stats));
    g_cache_count = 0;

    for (u32 i = 0; i < default_size_count && g_cache_count < SLAB_MAX_CACHES; i++) {
        char name[32];
        ksnprintf(name, sizeof(name), "slab%zu", default_sizes[i]);
        slab_cache_t *cache = slab_cache_create(name, default_sizes[i], default_sizes[i], NULL, NULL);
        if (!cache)
            klog_error("slab_init: failed to create cache for %zu bytes", default_sizes[i]);
    }
}

void *slab_alloc(size_t size) {
    if (size == 0 || size > SLAB_MAX_SIZE)
        return NULL;

    slab_cache_t *cache = find_best_cache(size);
    if (!cache) {
        g_stats.failed_allocs++;
        return NULL;
    }

    return slab_cache_alloc(cache);
}

void slab_free(void *ptr) {
    if (!ptr)
        return;

    slab_cache_t *cache = slab_find_cache_for_ptr(ptr);
    if (!cache) {
        klog_error("slab_free: invalid pointer %p", ptr);
        return;
    }

    slab_cache_free(cache, ptr);
}

/* ----------------------------------------------------------- */
/*                   Cache Management API                       */
/* ----------------------------------------------------------- */

slab_cache_t *slab_cache_create(const char *name, size_t object_size, size_t align,
                               slab_ctor_t ctor, slab_dtor_t dtor) {
    if (g_cache_count >= SLAB_MAX_CACHES || object_size == 0)
        return NULL;

    if (align == 0)
        align = 1;
    if (align & (align - 1))
        return NULL; /* align must be power of two */

    if (object_size < sizeof(slab_object_t))
        object_size = sizeof(slab_object_t);

    object_size = (object_size + align - 1) & ~(align - 1);

    size_t pages = (sizeof(slab_cache_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    void *page = pmm_alloc_pages(pages);
    if (!page)
        return NULL;
    slab_cache_t *cache = (slab_cache_t *)VMM_X86_64_PHYS_TO_VIRT((u64)page);
    memset(cache, 0, sizeof(*cache));

    if (name)
        strncpy(cache->name, name, sizeof(cache->name) - 1);
    cache->object_size = object_size;
    cache->align = align;
    cache->ctor = ctor;
    cache->dtor = dtor;
    list_init(&cache->full_slabs);
    list_init(&cache->partial_slabs);
    list_init(&cache->empty_slabs);
    spinlock_init(&cache->lock);
    cache->magic = SLAB_MAGIC_CACHE;

    g_caches[g_cache_count++] = cache;
    g_stats.total_caches++;
    g_stats.overhead_memory += pages * PAGE_SIZE;

    return cache;
}

bool slab_cache_destroy(slab_cache_t *cache) {
    if (!cache)
        return false;

    if (!list_is_empty(&cache->full_slabs) || !list_is_empty(&cache->partial_slabs) ||
        !list_is_empty(&cache->empty_slabs) || cache->allocated_objects != 0)
        return false;

    for (u32 i = 0; i < g_cache_count; i++) {
        if (g_caches[i] == cache) {
            for (u32 j = i + 1; j < g_cache_count; j++)
                g_caches[j - 1] = g_caches[j];
            g_cache_count--;
            break;
        }
    }

    size_t pages = (sizeof(slab_cache_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    void *phys = (void *)VMM_X86_64_VIRT_TO_PHYS(cache);
    pmm_free_pages(phys, pages);
    g_stats.total_caches--;
    if (g_stats.overhead_memory >= pages * PAGE_SIZE)
        g_stats.overhead_memory -= pages * PAGE_SIZE;
    return true;
}

void *slab_cache_alloc(slab_cache_t *cache) {
    if (!cache)
        return NULL;

    spinlock_lock(&cache->lock);

    slab_t *slab = NULL;
    if (!list_is_empty(&cache->partial_slabs)) {
        slab = LIST_ENTRY(cache->partial_slabs.next, slab_t, node);
    } else if (!list_is_empty(&cache->empty_slabs)) {
        slab = LIST_ENTRY(cache->empty_slabs.next, slab_t, node);
        list_remove(&slab->node);
        list_insert_after(&cache->partial_slabs, &slab->node);
    } else {
        slab = create_slab(cache);
        if (!slab) {
            g_stats.failed_allocs++;
            spinlock_unlock(&cache->lock);
            return NULL;
        }
        list_insert_after(&cache->partial_slabs, &slab->node);
    }

    slab_object_t *obj = slab->free_list;
    slab->free_list = obj->next_free;
    slab->free_objects--;

    if (slab->free_objects == 0) {
        list_remove(&slab->node);
        list_insert_after(&cache->full_slabs, &slab->node);
    }

    cache->allocated_objects++;
    cache->alloc_count++;
    g_stats.total_allocs++;
    g_stats.allocated_memory += cache->object_size;

    slab_ctor_t ctor = cache->ctor;
    size_t obj_size = cache->object_size;
    spinlock_unlock(&cache->lock);

    if (ctor)
        ctor(obj, obj_size);

    return obj;
}

void slab_cache_free(slab_cache_t *cache, void *ptr) {
    if (!cache || !ptr)
        return;

    spinlock_lock(&cache->lock);

    slab_t *slab = (slab_t *)((uintptr_t)ptr & ~(SLAB_PAGE_SIZE - 1));
    if (slab->magic != SLAB_MAGIC) {
        spinlock_unlock(&cache->lock);
        return;
    }

    slab_dtor_t dtor = cache->dtor;

    slab_object_t *obj = (slab_object_t *)ptr;
    obj->next_free = slab->free_list;
    slab->free_list = obj;
    slab->free_objects++;

    list_remove(&slab->node);
    if (slab->free_objects == slab->total_objects) {
        list_insert_after(&cache->empty_slabs, &slab->node);
    } else {
        list_insert_after(&cache->partial_slabs, &slab->node);
    }

    cache->allocated_objects--;
    cache->free_count++;
    g_stats.total_frees++;
    if (g_stats.allocated_memory >= cache->object_size)
        g_stats.allocated_memory -= cache->object_size;

    size_t obj_size = cache->object_size;
    spinlock_unlock(&cache->lock);

    if (dtor)
        dtor(ptr, obj_size);
}

/* ----------------------------------------------------------- */
/*                      Debug Helpers                          */
/* ----------------------------------------------------------- */

void slab_dump_caches(void) {
    for (u32 i = 0; i < g_cache_count; i++) {
        slab_cache_t *c = g_caches[i];
        klog_info("[slab] cache %u: %s obj=%zu alloc=%u slabs=%u", i, c->name,
                  c->object_size, c->allocated_objects, c->total_slabs);
    }
}

void slab_dump_cache(slab_cache_t *cache) {
    if (!cache)
        return;
    klog_info("[slab] cache %s: obj=%zu slabs=%u alloc=%u", cache->name,
              cache->object_size, cache->total_slabs, cache->allocated_objects);
}

void slab_get_stats(slab_stats_t *stats) {
    if (!stats)
        return;
    memcpy(stats, &g_stats, sizeof(*stats));
}

bool slab_check_integrity(slab_cache_t *cache) {
    if (cache) {
        list_node_t *it;
        LIST_FOR_EACH(it, &cache->full_slabs) {
            slab_t *s = LIST_ENTRY(it, slab_t, node);
            if (s->magic != SLAB_MAGIC || s->object_size != cache->object_size)
                return false;
        }
        LIST_FOR_EACH(it, &cache->partial_slabs) {
            slab_t *s = LIST_ENTRY(it, slab_t, node);
            if (s->magic != SLAB_MAGIC || s->object_size != cache->object_size)
                return false;
        }
        LIST_FOR_EACH(it, &cache->empty_slabs) {
            slab_t *s = LIST_ENTRY(it, slab_t, node);
            if (s->magic != SLAB_MAGIC || s->object_size != cache->object_size)
                return false;
        }
        return true;
    }

    for (u32 i = 0; i < g_cache_count; i++) {
        if (!slab_check_integrity(g_caches[i]))
            return false;
    }
    return true;
}

slab_cache_t *slab_find_cache_for_ptr(void *ptr) {
    if (!SLAB_PTR_VALID(ptr))
        return NULL;
    uintptr_t base = (uintptr_t)ptr & ~(SLAB_PAGE_SIZE - 1);
    slab_t *slab = (slab_t *)base;

    for (u32 i = 0; i < g_cache_count; i++) {
        slab_cache_t *cache = g_caches[i];
        list_node_t *it;
        LIST_FOR_EACH(it, &cache->full_slabs) {
            if (LIST_ENTRY(it, slab_t, node) == slab)
                return cache;
        }
        LIST_FOR_EACH(it, &cache->partial_slabs) {
            if (LIST_ENTRY(it, slab_t, node) == slab)
                return cache;
        }
        LIST_FOR_EACH(it, &cache->empty_slabs) {
            if (LIST_ENTRY(it, slab_t, node) == slab)
                return cache;
        }
    }
    return NULL;
}

u32 slab_shrink_cache(slab_cache_t *cache) {
    if (!cache) {
        u32 total = 0;
        for (u32 i = 0; i < g_cache_count; i++)
            total += slab_shrink_cache(g_caches[i]);
        return total;
    }

    u32 freed = 0;
    list_node_t *it, *tmp;
    LIST_FOR_EACH_SAFE(it, tmp, &cache->empty_slabs) {
        slab_t *slab = LIST_ENTRY(it, slab_t, node);
        list_remove(&slab->node);
        destroy_slab(cache, slab);
        freed++;
    }
    return freed;
}

u64 slab_reclaim_memory(u32 priority) {
    (void)priority;
    u32 freed_slabs = slab_shrink_cache(NULL);
    return (u64)freed_slabs * SLAB_PAGE_SIZE;
}

