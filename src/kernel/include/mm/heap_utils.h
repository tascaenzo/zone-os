#pragma once

#include <lib/types.h>
#include <mm/kernel_layout.h>

/**
 * @file mm/heap_utils.h
 * @brief Utility per la gestione della memoria heap del kernel
 *
 * Queste funzioni forniscono un layer minimo per riservare e mappare
 * pagine nell'area dedicata all'heap del kernel. L'allocatore vero e
 * proprio potrà costruirsi sopra queste primitive.
 */

/**
 * @brief Riserva un range virtuale contiguo nell'area heap
 *
 * La dimensione deve essere multipla di PAGE_SIZE. Nessuna pagina fisica
 * viene mappata in questa fase.
 *
 * @param size Dimensione richiesta in byte
 * @return Indirizzo virtuale base riservato oppure NULL se non c'è spazio
 */
void *heap_reserve_virtual_range(size_t size);

/**
 * @brief Libera un range virtuale precedentemente riservato
 *
 * Attualmente è un segnaposto per future estensioni. Non effettua alcuna
 * operazione reale.
 */
void heap_release_virtual_range(void *base, size_t size);

/**
 * @brief Mappa pagine fisiche all'interno di un range virtuale dell'heap
 *
 * @param virt_base Indirizzo virtuale dove iniziare la mappatura
 * @param page_count Numero di pagine consecutive da mappare
 * @return true se la mappatura è riuscita
 */
bool heap_map_physical_pages(void *virt_base, size_t page_count);

/**
 * @brief Rimuove la mappatura fisica da un range virtuale dell'heap
 *
 * Il range virtuale rimane riservato ma le pagine fisiche vengono liberate.
 */
void heap_unmap_physical_pages(void *virt_base, size_t page_count);

/**
 * @brief Verifica se un indirizzo ricade nell'area heap del kernel
 */
bool heap_address_valid(const void *addr);
