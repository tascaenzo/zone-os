#pragma once

#include <arch/memory.h>

/**
 * @file mm/kernel_layout.h
 * @brief Definizioni del layout di memoria virtuale del kernel
 *
 * Questo header raccoglie gli indirizzi base e le dimensioni delle
 * principali aree dello spazio virtuale del kernel. Tutti i valori sono
 * canonici per l'architettura x86_64 a 64 bit e devono rimanere
 * sincronizzati con lo script di linker.
 */

/* -------------------------------------------------------------------------- */
/*                               REGIONI BASE                                 */
/* -------------------------------------------------------------------------- */

/** Indirizzo virtuale di partenza dell'immagine del kernel */
#define KERNEL_TEXT_BASE 0xFFFFFFFF80000000UL

/** Base dell'area di direct mapping (fisico->virtuale) */
#define DIRECT_MAP_BASE  0xFFFF800000000000UL
/** Dimensione massima mappata dal direct mapping */
#define DIRECT_MAP_SIZE  (64UL * GB)

/** Base del kernel heap (allocazioni dinamiche) */
#define KERNEL_HEAP_BASE 0xFFFF888000000000UL
/** Dimensione complessiva riservata per l'heap */
#define KERNEL_HEAP_SIZE (16UL * GB)

/** Base dell'area vmalloc per mapping dinamici di driver/moduli */
#define VMALLOC_BASE     0xFFFFC88000000000UL
/** Dimensione riservata all'area vmalloc */
#define VMALLOC_SIZE     (16UL * GB)

/* -------------------------------------------------------------------------- */
/*                              MACRO UTILITARIE                              */
/* -------------------------------------------------------------------------- */

/** Converte un indirizzo fisico in virtuale nel direct mapping */
#define PHYS_TO_VIRT(addr) ((u64)(addr) + DIRECT_MAP_BASE)
/** Converte un indirizzo virtuale del direct mapping in fisico */
#define VIRT_TO_PHYS(addr) ((u64)(addr) - DIRECT_MAP_BASE)

/** Verifica se un indirizzo appartiene all'area heap del kernel */
#define IS_KERNEL_HEAP(addr)  \
    ((u64)(addr) >= KERNEL_HEAP_BASE &&
     (u64)(addr) < (KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE))

