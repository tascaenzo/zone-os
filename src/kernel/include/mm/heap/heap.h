#pragma once

#include <lib/stdbool.h>
#include <lib/types.h>

/**
 * @file mm/heap.h
 * @brief Hybrid Kernel Heap Interface (Slab + Buddy)
 *
 * Questo header definisce l'interfaccia per l'heap del kernel.
 * L'heap utilizza un approccio ibrido basato su due strategie:
 *
 * - Slab Allocator: per oggetti piccoli e frequenti (≤ 2KB)
 * - Buddy Allocator: per blocchi più grandi (≥ 2KB)
 *
 * Questo design massimizza efficienza, locality e riduce frammentazione.
 */

/*
 * ============================================================================
 * CONFIGURATION CONSTANTS
 * ============================================================================
 */

/* Soglia per decidere tra slab e buddy allocator */
#define HEAP_SLAB_THRESHOLD 2048 /* Oggetti ≤ 2KB vanno al slab */

/* Allineamenti standard */
#define HEAP_MIN_ALIGN 8 /* Allineamento minimo garantito */

/* Limiti dimensioni */
#define HEAP_MAX_ALLOC_SIZE (1ULL << 30) /* 1GB max per singola allocazione */

/* Flags per allocazioni speciali */
#define HEAP_FLAG_ZERO (1 << 0)   /* Azzera la memoria */
#define HEAP_FLAG_ATOMIC (1 << 1) /* Allocazione atomica (no sleep) */
#define HEAP_FLAG_DMA (1 << 2)    /* Memoria DMA-able */
#define HEAP_FLAG_ALIGN (1 << 3)  /* Usa allineamento custom */

/*
 * ============================================================================
 * DATA STRUCTURES
 * ============================================================================
 */

/**
 * @brief Statistiche globali dell'heap
 */
typedef struct {
  /* Statistiche generali */
  u64 total_memory;     /* Memoria totale gestita */
  u64 allocated_memory; /* Memoria attualmente allocata */
  u64 free_memory;      /* Memoria libera disponibile */
  u64 overhead_memory;  /* Overhead strutture interne */

  /* Contatori operazioni */
  u64 total_allocs;  /* Allocazioni totali */
  u64 total_frees;   /* Deallocazioni totali */
  u64 failed_allocs; /* Allocazioni fallite */

  /* Suddivisione slab vs buddy */
  u64 slab_allocs;  /* Allocazioni gestite da slab */
  u64 buddy_allocs; /* Allocazioni gestite da buddy */
  u64 slab_memory;  /* Memoria in uso dal slab */
  u64 buddy_memory; /* Memoria in uso dal buddy */

  /* Frammentazione */
  u32 fragmentation_percent; /* Percentuale frammentazione */
  u32 largest_free_block;    /* Blocco libero più grande */
} heap_stats_t;

/*
 * ============================================================================
 * CORE HEAP API
 * ============================================================================
 */

/**
 * @brief Inizializza l'heap del kernel (slab + buddy)
 *
 * Deve essere chiamata dopo l'inizializzazione del PMM.
 * Configura entrambi i sottosistemi e le strutture globali.
 */
void heap_init(void);

/**
 * @brief Alloca memoria dal kernel heap
 *
 * Automaticamente sceglie tra slab e buddy allocator basandosi
 * sulla dimensione richiesta.
 *
 * @param size Dimensione in byte richiesta
 * @return Puntatore a memoria valida o NULL in caso di errore
 *
 * @note La memoria ritornata è sempre allineata a HEAP_MIN_ALIGN
 * @note Per dimensioni ≤ HEAP_SLAB_THRESHOLD usa slab allocator
 * @note Per dimensioni > HEAP_SLAB_THRESHOLD usa buddy allocator
 */
void *kalloc(size_t size);

/**
 * @brief Alloca memoria con flags speciali
 *
 * @param size Dimensione richiesta
 * @param flags Combinazione di HEAP_FLAG_*
 * @param align Allineamento custom (se HEAP_FLAG_ALIGN è set)
 * @return Puntatore allocato o NULL
 *
 * @example
 *   void *dma_buf = kalloc_flags(4096, HEAP_FLAG_DMA | HEAP_FLAG_ZERO, 0);
 *   void *aligned = kalloc_flags(1024, HEAP_FLAG_ALIGN, 256);
 */
void *kalloc_flags(size_t size, u32 flags, size_t align);

/**
 * @brief Alloca e azzera memoria (equivalente a calloc)
 *
 * @param nmemb Numero di elementi
 * @param size Dimensione di ogni elemento
 * @return Puntatore a memoria azzerata o NULL
 */
void *kcalloc(size_t nmemb, size_t size);

/**
 * @brief Rialloca un blocco di memoria
 *
 * @param ptr Puntatore esistente (può essere NULL)
 * @param new_size Nuova dimensione richiesta
 * @return Nuovo puntatore o NULL
 *
 * @note Se ptr è NULL, equivale a kalloc(new_size)
 * @note Se new_size è 0, equivale a kfree(ptr) e ritorna NULL
 * @note Il contenuto esistente viene preservato fino a min(old_size, new_size)
 */
void *krealloc(void *ptr, size_t new_size);

/**
 * @brief Libera memoria precedentemente allocata
 *
 * @param ptr Puntatore restituito da kalloc/kcalloc/krealloc
 *
 * @note Sicuro chiamare con ptr == NULL (no-op)
 * @note Automaticamente determina se usare slab o buddy per la deallocazione
 */
void kfree(void *ptr);

/*
 * ============================================================================
 * STATISTICS AND INTROSPECTION API
 * ============================================================================
 */

/**
 * @brief Ottieni statistiche globali dell'heap
 *
 * @param stats Struttura da riempire con le statistiche
 */
void heap_get_stats(heap_stats_t *stats);

/**
 * @brief Stampa statistiche dettagliate dell'heap
 */
void heap_dump_stats(void);

/**
 * @brief Verifica l'integrità dell'heap
 *
 * @return true se l'heap è integro, false se corrotto
 */
bool heap_check_integrity(void);

/*
 * ============================================================================
 * MEMORY PRESSURE MANAGEMENT
 * ============================================================================
 */

/**
 * @brief Forza il rilascio di memoria inutilizzata
 *
 * @param priority Priorità del reclaim (0-100, dove 100 è massima urgenza)
 * @return Quantità di memoria liberata in bytes
 */
u64 heap_shrink(u32 priority);

/**
 * @brief Configura soglie di memory pressure
 *
 * @param low_watermark Soglia bassa (%) - inizia reclaim soft
 * @param high_watermark Soglia alta (%) - reclaim aggressivo
 */
void heap_set_watermarks(u32 low_watermark, u32 high_watermark);

/*
 * ============================================================================
 * SPECIALIZED ALLOCATION API
 * ============================================================================
 */

/**
 * @brief Alloca memoria DMA-coherent
 *
 * @param size Dimensione richiesta
 * @param dma_handle Puntatore per ricevere l'indirizzo DMA fisico
 * @return Puntatore virtuale o NULL
 */
void *heap_alloc_dma(size_t size, u64 *dma_handle);

/**
 * @brief Libera memoria DMA-coherent
 *
 * @param size Dimensione originale
 * @param vaddr Indirizzo virtuale
 * @param dma_handle Indirizzo DMA fisico
 */
void heap_free_dma(size_t size, void *vaddr, u64 dma_handle);

/**
 * @brief Alloca pagine contigue fisicamente
 *
 * @param order Ordine dell'allocazione (2^order pagine)
 * @param flags Flags di allocazione
 * @return Puntatore alle pagine o NULL
 */
void *heap_alloc_pages(u32 order, u32 flags);

/**
 * @brief Libera pagine contigue
 *
 * @param ptr Puntatore ritornato da heap_alloc_pages
 * @param order Ordine originale dell'allocazione
 */
void heap_free_pages(void *ptr, u32 order);

/*
 * ============================================================================
 * UTILITY MACROS
 * ============================================================================
 */

/**
 * @brief Macro per allocation con type safety
 */
#define kalloc_type(type) ((type *)kalloc(sizeof(type)))
#define kcalloc_type(type, count) ((type *)kcalloc((count), sizeof(type)))

/**
 * @brief Macro per allocation array con type safety
 */
#define kalloc_array(type, count) ((type *)kalloc((count) * sizeof(type)))

/**
 * @brief Macro per realloc con type safety
 */
#define krealloc_type(ptr, type, count) ((type *)krealloc((ptr), (count) * sizeof(type)))

/**
 * @brief Allinea dimensione al multiplo più vicino
 */
#define HEAP_ALIGN_SIZE(size, align) (((size) + (align) - 1) & ~((align) - 1))