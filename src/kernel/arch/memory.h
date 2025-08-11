/**
 * @file include/arch/memory.h
 * @brief API memoria portabile per ZONE-OS: rilevazione mappa fisica e statistiche
 *
 * Espone tipologie di memoria e funzioni per inizializzare la rilevazione
 * e ottenere regioni fisiche e statistiche aggregate in modo neutro rispetto all’architettura.
 *
 * Tipi comuni coperti:
 *  - Usable, Reserved, ACPI (Reclaim/NVS), Bad, Bootloader Reclaimable, Kernel, Framebuffer, MMIO
 *
 * @author Enzo Tasca
 * @date 2025
 * @license MIT
 */

#pragma once
#include <lib/stdbool.h>
#include <lib/stddef.h> /* size_t */
#include <lib/stdint.h> /* uint64_t */

/* Limite difensivo per chiamanti che usano buffer statici */
#ifndef ARCH_MAX_MEMORY_REGIONS
#define ARCH_MAX_MEMORY_REGIONS 512
#endif

typedef enum {
  ARCH_MEM_USABLE = 1,
  ARCH_MEM_RESERVED,
  ARCH_MEM_ACPI_RECLAIM,
  ARCH_MEM_ACPI_NVS,
  ARCH_MEM_BAD,
  ARCH_MEM_BOOT_RECLAIM,
  ARCH_MEM_KERNEL,
  ARCH_MEM_FRAMEBUFFER,
  ARCH_MEM_MMIO,
  ARCH_MEM_TYPE_COUNT /* sentinella */
} arch_mem_type_t;

typedef struct {
  uint64_t base;   /* indirizzo fisico base (byte) */
  uint64_t length; /* dimensione (byte) */
  arch_mem_type_t type;
} arch_mem_region_t;

/**
 * @brief Inizializza la rilevazione della memoria fisica (backend arch).
 * Deve essere chiamata una sola volta prima delle altre funzioni di questo header.
 */
void arch_memory_init(void);

/**
 * @brief Popola fino a @p max regioni in @p out e restituisce il numero scritto.
 * Le regioni restituite sono allineate e non sovrapposte secondo la policy dell'arch.
 * @return Numero di regioni copiate in @p out (<= max).
 */
size_t arch_memory_detect_regions(arch_mem_region_t *out, size_t max);

/**
 * @brief Restituisce memoria totale e utilizzabile (in byte).
 * I parametri possono essere NULL se non interessano.
 */
void arch_memory_get_stats(uint64_t *total, uint64_t *usable);

/**
 * @brief Dimensione di pagina del core (es. 4096 su x86_64).
 */
uint64_t arch_memory_page_size(void);
