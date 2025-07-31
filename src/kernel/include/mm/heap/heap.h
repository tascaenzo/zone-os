#pragma once

#include <lib/stdbool.h>
#include <lib/types.h>

/**
 * @file mm/heap/heap.h
 * @brief Interfaccia unificata per l’allocazione dinamica del kernel
 *
 * Fornisce funzioni di allocazione/deallocazione usate dal kernel,
 * astratte rispetto alla strategia sottostante (slab o buddy).
 *
 * STRATEGIA:
 * - Allocazioni ≤ 2048 byte → slab allocator
 * - Allocazioni > 2048 byte → buddy allocator
 *
 * TUTTE le allocazioni restituiscono puntatori validi nello spazio virtuale.
 *
 * NOTE:
 * - Internamente utilizza mapping fisico ↔ virtuale per blocchi allocati via buddy.
 * - La logica di fallback e threshold è hardcoded (ma estendibile).
 */

/**
 * @brief Inizializza il sistema heap del kernel
 *
 * Inizializza slab allocator e buddy allocator.
 * Richiede che PMM, VMM e logging siano già inizializzati.
 */
void heap_init(void);

/**
 * @brief Alloca memoria dinamica nel kernel
 *
 * @param size Dimensione richiesta in byte
 * @return Puntatore virtuale alla memoria allocata, o NULL su errore
 *
 * @note Il contenuto non è inizializzato (spazzatura)
 */
void *kmalloc(size_t size);

/**
 * @brief Alloca e azzera memoria dinamica
 *
 * @param nmemb Numero di elementi
 * @param size Dimensione per elemento
 * @return Puntatore alla memoria azzerata, o NULL su errore
 */
void *kcalloc(size_t nmemb, size_t size);

/**
 * @brief Libera memoria precedentemente allocata
 *
 * @param ptr Puntatore da liberare
 */
void kfree(void *ptr);

/**
 * @brief Stampa stato corrente di heap, slab e buddy
 */
void heap_dump_info(void);

/**
 * @brief Verifica integrità degli allocator heap
 *
 * @return true se tutti i sottosistemi heap risultano coerenti
 */
bool heap_check_integrity(void);
