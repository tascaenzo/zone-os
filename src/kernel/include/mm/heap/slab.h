#pragma once

#include <klib/list.h>
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

/*
 * ============================================================================
 * CONFIGURATION CONSTANTS
 * ============================================================================
 */

/* Dimensioni cache predefinite (potenze di 2 + alcune intermedie) */
#define SLAB_MIN_SIZE 16   /* Oggetto più piccolo: 16 bytes */
#define SLAB_MAX_SIZE 2048 /* Oggetto più grande: 2KB */

/* Numero massimo di cache nel sistema */
#define SLAB_MAX_CACHES 32

/* Dimensione di una slab page (tipicamente una page fisica) */
#define SLAB_PAGE_SIZE 4096

/* Numero massimo di oggetti per slab (per arrays statici) */
#define SLAB_MAX_OBJECTS 256

/* Magic numbers per debugging */
#define SLAB_MAGIC_ALLOC 0xABCDEF01 /* Oggetto allocato */
#define SLAB_MAGIC_FREE 0xDEADBEEF  /* Oggetto libero */
#define SLAB_MAGIC_CACHE 0xCAFEBABE /* Cache valida */

/*
 * ============================================================================
 * DATA STRUCTURES
 * ============================================================================
 */

/**
 * @brief Singolo oggetto in una slab
 *
 * Quando libero, contiene un puntatore al prossimo oggetto libero.
 * Quando allocato, è memory dell'utente.
 */
typedef union slab_object {
  union slab_object *next_free; /* Puntatore al prossimo libero (quando free) */
  u8 data[0];                   /* Inizio dati utente (quando allocato) */
} slab_object_t;

/**
 * @brief Una singola slab page
 *
 * Contiene multipli oggetti della stessa dimensione.
 * Traccia oggetti liberi e allocati.
 */
typedef struct slab {
  list_node_t node; /* Lista di slab nella cache */

  void *page_addr;          /* Indirizzo della page fisica (4KB) */
  slab_object_t *free_list; /* Lista oggetti liberi in questa slab */

  u16 total_objects; /* Oggetti totali in questa slab */
  u16 free_objects;  /* Oggetti attualmente liberi */
  u16 object_size;   /* Dimensione di ogni oggetto */

  u32 magic; /* Magic per debugging */
} slab_t;

/**
 * @brief Constructor/destructor per oggetti
 */
typedef void (*slab_ctor_t)(void *object, size_t size);
typedef void (*slab_dtor_t)(void *object, size_t size);

/**
 * @brief Cache per oggetti di dimensione fissa
 *
 * Mantiene lista di slab pages, alcune piene, alcune parziali, alcune vuote.
 */
typedef struct slab_cache {
  char name[32];      /* Nome della cache (per debug) */
  size_t object_size; /* Dimensione oggetti in questa cache */
  size_t align;       /* Allineamento richiesto */

  /* Constructor/destructor (opzionali) */
  slab_ctor_t ctor; /* Costruttore oggetti */
  slab_dtor_t dtor; /* Distruttore oggetti */

  /* Liste slab per stato */
  list_node_t full_slabs;    /* Slab completamente piene */
  list_node_t partial_slabs; /* Slab parzialmente piene */
  list_node_t empty_slabs;   /* Slab vuote (candidate per free) */

  /* Statistiche */
  u32 total_slabs;       /* Numero totale slab */
  u32 total_objects;     /* Oggetti totali nella cache */
  u32 allocated_objects; /* Oggetti attualmente allocati */
  u64 alloc_count;       /* Contatore allocazioni */
  u64 free_count;        /* Contatore deallocazioni */

  /* Cache coloring per ottimizzazione CPU cache */
  u32 color_offset; /* Offset colore corrente */
  u32 color_range;  /* Range colori disponibili */

  u32 magic; /* Magic per debugging */
} slab_cache_t;

/**
 * @brief Statistiche globali del sistema slab
 */
typedef struct {
  u32 total_caches;     /* Cache attive */
  u32 total_slabs;      /* Slab pages totali */
  u64 total_memory;     /* Memoria totale usata (bytes) */
  u64 allocated_memory; /* Memoria allocata agli utenti */
  u64 overhead_memory;  /* Overhead strutture interne */
  u64 total_allocs;     /* Allocazioni totali */
  u64 total_frees;      /* Deallocazioni totali */
  u64 failed_allocs;    /* Allocazioni fallite */
} slab_stats_t;

/*
 * ============================================================================
 * CORE API
 * ============================================================================
 */

/**
 * @brief Inizializza il sottosistema slab allocator
 *
 * Crea le cache predefinite per dimensioni standard e inizializza
 * le strutture dati globali.
 *
 * @note Deve essere chiamata dopo l'inizializzazione del PMM/Buddy allocator
 */
void slab_init(void);

/**
 * @brief Alloca un oggetto di dimensione specificata
 *
 * Trova la cache appropriata per la dimensione e alloca un oggetto.
 * Se necessario, crea nuove slab pages.
 *
 * @param size Dimensione richiesta (≤ SLAB_MAX_SIZE)
 * @return Puntatore all'oggetto allocato, o NULL se fallisce
 *
 * @note L'oggetto ritornato è allineato appropriatamente per la dimensione
 * @note Se esiste un costruttore per la cache, viene chiamato automaticamente
 */
void *slab_alloc(size_t size);

/**
 * @brief Libera un oggetto precedentemente allocato
 *
 * Restituisce l'oggetto alla sua slab e aggiorna le statistiche.
 * Se la slab diventa vuota, può essere rilasciata al buddy allocator.
 *
 * @param ptr Puntatore ritornato da slab_alloc() (deve essere non-NULL)
 *
 * @note Se esiste un distruttore per la cache, viene chiamato automaticamente
 * @note Passing NULL è safe (no-op)
 */
void slab_free(void *ptr);

/*
 * ============================================================================
 * CACHE MANAGEMENT API
 * ============================================================================
 */

/**
 * @brief Crea una cache custom per oggetti specifici
 *
 * Utile per strutture kernel con pattern di allocazione specifici.
 *
 * @param name Nome della cache (per debug/stats)
 * @param object_size Dimensione di ogni oggetto
 * @param align Allineamento richiesto (deve essere potenza di 2)
 * @param ctor Costruttore (opzionale, può essere NULL)
 * @param dtor Distruttore (opzionale, può essere NULL)
 * @return Puntatore alla cache creata, o NULL se fallisce
 *
 * @example
 *   slab_cache_t *task_cache = slab_cache_create("task_struct",
 *                                                sizeof(task_struct),
 *                                                __alignof__(task_struct),
 *                                                task_ctor, task_dtor);
 */
slab_cache_t *slab_cache_create(const char *name, size_t object_size, size_t align, slab_ctor_t ctor, slab_dtor_t dtor);

/**
 * @brief Distrugge una cache custom
 *
 * @param cache Cache da distruggere (deve essere vuota)
 * @return true se successo, false se cache non vuota
 */
bool slab_cache_destroy(slab_cache_t *cache);

/**
 * @brief Alloca da una cache specifica
 *
 * @param cache Cache da cui allocare
 * @return Oggetto allocato o NULL
 */
void *slab_cache_alloc(slab_cache_t *cache);

/**
 * @brief Libera oggetto in una cache specifica
 *
 * @param cache Cache target
 * @param ptr Oggetto da liberare
 */
void slab_cache_free(slab_cache_t *cache, void *ptr);

/*
 * ============================================================================
 * DEBUGGING AND INTROSPECTION API
 * ============================================================================
 */

/**
 * @brief Stampa informazioni su tutte le cache
 */
void slab_dump_caches(void);

/**
 * @brief Stampa informazioni dettagliate su una cache
 *
 * @param cache Cache da ispezionare
 */
void slab_dump_cache(slab_cache_t *cache);

/**
 * @brief Ottiene statistiche globali del sistema slab
 *
 * @param stats Struttura da riempire con le statistiche
 */
void slab_get_stats(slab_stats_t *stats);

/**
 * @brief Verifica integrità di una cache
 *
 * @param cache Cache da verificare (NULL = tutte le cache)
 * @return true se integra, false se corrotta
 */
bool slab_check_integrity(slab_cache_t *cache);

/**
 * @brief Trova la cache che contiene un dato puntatore
 *
 * Utile per debugging e per implementare free() senza size.
 *
 * @param ptr Puntatore da cercare
 * @return Cache che contiene il puntatore, o NULL se non trovato
 */
slab_cache_t *slab_find_cache_for_ptr(void *ptr);

/*
 * ============================================================================
 * MEMORY PRESSURE AND RECLAIM API
 * ============================================================================
 */

/**
 * @brief Rilascia slab vuote per ridurre uso memoria
 *
 * @param cache Cache target (NULL = tutte le cache)
 * @return Numero di slab rilasciate
 */
u32 slab_shrink_cache(slab_cache_t *cache);

/**
 * @brief Funzione di callback per memory pressure
 *
 * Chiamata dal kernel quando la memoria si sta esaurendo.
 *
 * @param priority Priorità del reclaim (0 = low, 100 = critical)
 * @return Memoria liberata in bytes
 */
u64 slab_reclaim_memory(u32 priority);

/*
 * ============================================================================
 * UTILITY MACROS
 * ============================================================================
 */

/**
 * @brief Calcola il numero di oggetti che entrano in una slab page
 */
#define SLAB_OBJECTS_PER_PAGE(obj_size) ((SLAB_PAGE_SIZE - sizeof(slab_t)) / (obj_size))

/**
 * @brief Verifica se un puntatore sembra valido per lo slab
 */
#define SLAB_PTR_VALID(ptr) ((ptr) != NULL && ((uintptr_t)(ptr) & 0x3) == 0)

/**
 * @brief Macro per definire costruttori/distruttori type-safe
 */
#define SLAB_CTOR(name, type)                                                                                                                                                      \
  static void name(void *obj, size_t size) {                                                                                                                                       \
    type *typed_obj = (type *)obj;                                                                                                                                                 \
    (void)size; /* Suppress unused warning */                                                                                                                                      \
                /* Initialization code here */                                                                                                                                     \
  }

#define SLAB_DTOR(name, type)                                                                                                                                                      \
  static void name(void *obj, size_t size) {                                                                                                                                       \
    type *typed_obj = (type *)obj;                                                                                                                                                 \
    (void)size; /* Suppress unused warning */                                                                                                                                      \
                /* Cleanup code here */                                                                                                                                            \
  }
