#pragma once

#include <klib/bitmap.h>
#include <klib/list.h>
#include <lib/stdbool.h>
#include <lib/types.h>

/**
 * @file mm/heap/buddy.h
 * @brief Buddy Allocator - Interfaccia pubblica
 *
 * Implementazione di un gestore di memoria fisica basato su buddy system.
 * Gestisce blocchi in potenze di 2, tracciati per ordine di grandezza.
 *
 * DESIGN:
 * - Ordine minimo: 12 (4KB pages)
 * - Ordine massimo: 16 (64KB blocks)
 * - Range: 4KB - 64KB in potenze di 2
 * - Coalescing automatico dei blocchi buddy
 * - Bitmap per tracking allocazioni
 *
 * UTILIZZO TIPICO:
 * - Heap kernel per kmalloc()
 * - Allocazione page table
 * - Buffer DMA contigui
 * - Stack kernel
 */

/*
 * ============================================================================
 * CONFIGURATION CONSTANTS
 * ============================================================================
 */

#define BUDDY_MIN_ORDER 12
#define BUDDY_MAX_ORDER 20

/* Numero totale di ordini gestiti */
#define BUDDY_ORDER_COUNT (BUDDY_MAX_ORDER - BUDDY_MIN_ORDER + 1)

/* Dimensioni comode per calcoli */
#define BUDDY_MIN_BLOCK_SIZE (1UL << BUDDY_MIN_ORDER) /* 4KB */
#define BUDDY_MAX_BLOCK_SIZE (1UL << BUDDY_MAX_ORDER) /* 64KB */

/* Verifica validità ordine */
#define BUDDY_VALID_ORDER(order) ((order) >= BUDDY_MIN_ORDER && (order) <= BUDDY_MAX_ORDER)

/*
 * ============================================================================
 * DATA STRUCTURES
 * ============================================================================
 */

/**
 * @brief Struttura per un blocco libero nelle free lists
 *
 * IMPORTANTE: Questa struttura viene memorizzata all'inizio
 * di ogni blocco libero. Per questo il BUDDY_MIN_ORDER deve
 * essere almeno sizeof(buddy_block_t).
 */
typedef struct buddy_block {
  list_node_t node; /* Nodo per concatenamento in free list */
  u8 order;         /* Ordine (dimensione) di questo blocco */
  u8 _reserved[3];  /* Padding per allineamento */
} buddy_block_t;

/**
 * @brief Allocatore buddy completo
 *
 * Gestisce una regione contigua di memoria fisica,
 * suddividendola in blocchi di varie dimensioni.
 */
typedef struct {
  u64 base_addr;  /* Indirizzo base della regione gestita */
  u64 total_size; /* Dimensione totale in bytes */

  list_node_t free_lists[BUDDY_MAX_ORDER + 1]; /* Free lists per ogni ordine [0..BUDDY_MAX_ORDER] */
  bitmap_t allocation_map;                     /* Bitmap per tracciare blocchi allocati */

  /* Statistiche (opzionali per debug) */
  u64 total_allocs;  /* Numero totale di allocazioni */
  u64 total_frees;   /* Numero totale di deallocazioni */
  u64 failed_allocs; /* Allocazioni fallite (OOM) */
} buddy_allocator_t;

/*
 * ============================================================================
 * CORE API
 * ============================================================================
 */

/**
 * @brief Inizializza l'allocatore buddy
 *
 * Prepara l'allocatore per gestire una regione di memoria fisica.
 * La regione viene inizialmente aggiunta come blocchi liberi del
 * massimo ordine possibile.
 *
 * @param allocator Puntatore alla struttura allocatore (deve essere pre-allocata)
 * @param base_addr Indirizzo fisico base della regione (deve essere allineato a 4KB)
 * @param size_in_bytes Dimensione totale della regione (multiplo di 4KB)
 * @param bitmap_storage Buffer per bitmap interna (già allocato dal chiamante)
 * @param bitmap_bits Numero di bit disponibili nel buffer bitmap
 *
 * @note Il bitmap_storage deve essere abbastanza grande per contenere
 *       almeno (size_in_bytes / BUDDY_MIN_BLOCK_SIZE) bit.
 *
 * @note La funzione allinea automaticamente la size_in_bytes verso il basso
 *       al multiplo più vicino di BUDDY_MIN_BLOCK_SIZE.
 */
void buddy_init(buddy_allocator_t *allocator, u64 base_addr, u64 size_in_bytes, u64 *bitmap_storage, size_t bitmap_bits);

/**
 * @brief Alloca un blocco di memoria contigua
 *
 * Trova il blocco più piccolo che può contenere la dimensione richiesta.
 * Se necessario, splitta blocchi più grandi. Utilizza algoritmo first-fit.
 *
 * @param allocator Allocatore buddy inizializzato
 * @param size Dimensione richiesta in bytes (verrà arrotondata alla prossima potenza di 2)
 * @return Indirizzo fisico del blocco allocato, o 0 se allocazione fallisce
 *
 * @note La dimensione effettiva allocata sarà sempre una potenza di 2
 *       >= size e >= BUDDY_MIN_BLOCK_SIZE.
 *
 * @example
 *   u64 addr = buddy_alloc(&allocator, 8192);  // Alloca 8KB
 *   if (addr) {
 *       // Usa memoria all'indirizzo 'addr'
 *   }
 */
u64 buddy_alloc(buddy_allocator_t *allocator, size_t size);

/**
 * @brief Libera un blocco precedentemente allocato
 *
 * Libera il blocco e tenta di coalescere (unire) con blocchi buddy
 * adiacenti per ridurre la frammentazione.
 *
 * @param allocator Allocatore buddy
 * @param addr Indirizzo fisico del blocco (deve essere quello ritornato da buddy_alloc)
 * @param size Dimensione originale richiesta nell'allocazione
 *
 * @note È FONDAMENTALE passare la stessa size usata in buddy_alloc().
 *       Size sbagliata può causare corruzione dell'allocatore.
 *
 * @note La funzione verifica automaticamente l'integrità e logga errori
 *       se rileva parametri inconsistenti.
 *
 * @example
 *   buddy_free(&allocator, addr, 8192);  // Libera blocco da 8KB
 */
void buddy_free(buddy_allocator_t *allocator, u64 addr, size_t size);

/*
 * ============================================================================
 * DEBUGGING AND INTROSPECTION API
 * ============================================================================
 */

/**
 * @brief Stampa stato dettagliato dell'allocatore
 *
 * Mostra per ogni ordine:
 * - Numero di blocchi liberi
 * - Memoria totale libera per ordine
 * - Utilizzo complessivo della memoria
 */
void buddy_dump(buddy_allocator_t *allocator);

/**
 * @brief Verifica integrità dell'allocatore
 *
 * Controlla:
 * - Coerenza bitmap vs free lists
 * - Allineamento di tutti i blocchi
 * - Validità dei puntatori nelle liste
 *
 * @param allocator Allocatore da verificare
 * @return true se integro, false se rileva corruzione
 *
 * @note Da chiamare periodicamente in debug per rilevare bug early
 */
bool buddy_check_integrity(buddy_allocator_t *allocator);

/**
 * @brief Ottiene statistiche sull'utilizzo
 *
 * @param allocator Allocatore
 * @param total_free[out] Memoria totale libera in bytes (può essere NULL)
 * @param largest_free[out] Blocco libero più grande in bytes (può essere NULL)
 * @param fragmentation[out] Indice di frammentazione 0-100% (può essere NULL)
 */
void buddy_get_stats(buddy_allocator_t *allocator, u64 *total_free, u64 *largest_free, u32 *fragmentation);

/*
 * ============================================================================
 * UTILITY MACROS
 * ============================================================================
 */

/**
 * @brief Calcola l'ordine necessario per una dimensione
 */
#define BUDDY_ORDER_FOR_SIZE(size) ((size) <= BUDDY_MIN_BLOCK_SIZE ? BUDDY_MIN_ORDER : (BUDDY_MIN_ORDER + __builtin_clzl(BUDDY_MIN_BLOCK_SIZE) - __builtin_clzl((size) - 1)))

/**
 * @brief Calcola la dimensione di un blocco dato l'ordine
 */
#define BUDDY_BLOCK_SIZE(order) (1UL << (order))

/**
 * @brief Verifica se una dimensione è una potenza di 2 valida per il buddy
 */
#define BUDDY_IS_VALID_SIZE(size) ((size) >= BUDDY_MIN_BLOCK_SIZE && (size) <= BUDDY_MAX_BLOCK_SIZE && ((size) & ((size) - 1)) == 0)
