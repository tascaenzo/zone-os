#pragma once

#include <klib/list.h>
#include <klib/spinlock.h>
#include <lib/types.h>

/**
 * @file mm/heap/slab.h
 * @brief Slab Allocator - High-performance object caching
 *
 * Allocatore ottimizzato per oggetti piccoli e frequenti nel kernel.
 * Basato sul design di Bonwick (Solaris) con cache per classi di dimensione fissa.
 *
 * DESIGN PRINCIPLES:
 * - Cache separate per dimensioni comuni (16, 32, 64, ..., 2048 bytes)
 * - Slab pages da 4KB contenenti multipli oggetti della stessa dimensione
 * - Free list per allocazione/deallocazione O(1)
 * - Cache coloring per ottimizzazione cache CPU
 * - Costruttori/distruttori per inizializzazione oggetti
 *
 * UTILIZZO TIPICO:
 * - kmalloc() per allocazioni < 2KB
 * - Strutture kernel frequenti (task_struct, file_struct, etc.)
 * - Buffer di rete, oggetti filesystem
 */

/* ==========================================================================
 * CONFIGURATION CONSTANTS
 * ==========================================================================
 */

#define SLAB_MIN_SIZE 16     /* Oggetto più piccolo: 16 bytes */
#define SLAB_MAX_SIZE 2048   /* Oggetto più grande: 2KB */
#define SLAB_MAX_CACHES 32   /* Numero massimo di cache nel sistema */
#define SLAB_PAGE_SIZE 4096  /* Dimensione di una slab page */
#define SLAB_MAX_OBJECTS 256 /* Massimo oggetti per slab (limite statico) */

#define SLAB_MAGIC_ALLOC 0xABCDEF01 /* Magic oggetto allocato */
#define SLAB_MAGIC_FREE 0xDEADBEEF  /* Magic oggetto libero */
#define SLAB_MAGIC_CACHE 0xCAFEBABE /* Magic identificazione cache */

/* ==========================================================================
 * DATA STRUCTURES
 * ==========================================================================
 */

typedef struct slab slab_t;
typedef struct slab_cache slab_cache_t;

/**
 * @brief Singolo oggetto in una slab
 */
typedef union slab_object {
  union slab_object *next_free; /* Puntatore al prossimo libero (quando free) */
  u8 data[0];                   /* Inizio dati utente (quando allocato) */
} slab_object_t;

/**
 * @brief Una singola slab page
 */
struct slab {
  list_node_t node;         /* Nodo nelle liste della cache */
  void *page_addr;          /* Indirizzo fisico della slab */
  slab_object_t *free_list; /* Lista oggetti liberi */
  u16 total_objects;        /* Totale oggetti nella slab */
  u16 free_objects;         /* Numero oggetti liberi */
  u16 object_size;          /* Dimensione singolo oggetto */
  u32 magic;                /* Magic per validazione e debug runtime */
};

/**
 * @brief Constructor/destructor per oggetti
 */
typedef void (*slab_ctor_t)(void *object, size_t size);
typedef void (*slab_dtor_t)(void *object, size_t size);

/**
 * @brief Cache per oggetti di dimensione fissa
 */
struct slab_cache {
  char name[32];             /* Nome della cache (debug) */
  size_t object_size;        /* Dimensione oggetti */
  size_t align;              /* Allineamento */
  slab_ctor_t ctor;          /* Costruttore (opzionale) */
  slab_dtor_t dtor;          /* Distruttore (opzionale) */
  list_node_t full_slabs;    /* Slab completamente piene */
  list_node_t partial_slabs; /* Slab parzialmente piene */
  list_node_t empty_slabs;   /* Slab vuote */
  u32 total_slabs;           /* Slab totali */
  u32 total_objects;         /* Oggetti totali */
  u32 allocated_objects;     /* Oggetti attualmente allocati */
  u64 alloc_count;           /* Numero allocazioni */
  u64 free_count;            /* Numero deallocazioni */
  u32 color_offset;          /* Offset corrente (cache coloring) */
  u32 color_range;           /* Range colori disponibili */
  spinlock_t lock;           /* Lock della cache */
  u32 magic;                 /* Magic per validazione */
};

/**
 * @brief Statistiche globali del sistema slab
 */
typedef struct {
  u32 total_caches;
  u32 total_slabs;
  u64 total_memory;
  u64 allocated_memory;
  u64 overhead_memory;
  u64 total_allocs;
  u64 total_frees;
  u64 failed_allocs;
} slab_stats_t;

/* ==========================================================================
 * CORE API
 * ==========================================================================
 */

void slab_init(void);
void *slab_alloc(size_t size);
void slab_free(void *ptr);

/* ==========================================================================
 * CACHE MANAGEMENT API
 * ==========================================================================
 */

slab_cache_t *slab_cache_create(const char *name, size_t object_size, size_t align, slab_ctor_t ctor, slab_dtor_t dtor);
bool slab_cache_destroy(slab_cache_t *cache);
void *slab_cache_alloc(slab_cache_t *cache);
void slab_cache_free(slab_cache_t *cache, void *ptr);

/* ==========================================================================
 * DEBUGGING AND INTROSPECTION API
 * ==========================================================================
 */

void slab_dump_caches(void);
void slab_dump_cache(slab_cache_t *cache);
void slab_get_stats(slab_stats_t *stats);
bool slab_check_integrity(slab_cache_t *cache);
slab_cache_t *slab_find_cache_for_ptr(void *ptr);

/* ==========================================================================
 * MEMORY PRESSURE AND RECLAIM API
 * ==========================================================================
 */

u32 slab_shrink_cache(slab_cache_t *cache);
u64 slab_reclaim_memory(u32 priority);

/* ==========================================================================
 * UTILITY MACROS
 * ==========================================================================
 */

#define SLAB_OBJECTS_PER_PAGE(obj_size) ((SLAB_PAGE_SIZE - sizeof(slab_t)) / (obj_size))
#define SLAB_PTR_VALID(ptr) ((ptr) != NULL && ((uintptr_t)(ptr) & 0x3) == 0)

#define SLAB_CTOR(name, type)                                                                                                                                                      \
  static void name(void *obj, size_t size) {                                                                                                                                       \
    type *typed_obj = (type *)obj;                                                                                                                                                 \
    (void)size;                                                                                                                                                                    \
  }

#define SLAB_DTOR(name, type)                                                                                                                                                      \
  static void name(void *obj, size_t size) {                                                                                                                                       \
    type *typed_obj = (type *)obj;                                                                                                                                                 \
    (void)size;                                                                                                                                                                    \
  }

slab_cache_t *slab_find_cache_for_size(size_t size);

/* ==========================================================================
 * EXTERNAL DATA (usati da heap.c)
 * ==========================================================================
 */

extern slab_cache_t slab_caches[SLAB_MAX_CACHES];
extern u32 slab_cache_count;
