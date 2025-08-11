/**
 * @file include/mm/page.h
 * @brief Helper generici per gestione pagine (arch‑agnostici)
 *
 * Fornisce utilità per allineamento e conversioni pagina↔indirizzo.
 * Si appoggia a arch_memory_page_size() esposta da <arch/memory.h>.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once
#include <arch/memory.h>
#include <lib/stdbool.h>
#include <lib/stdint.h>

/* Dimensione pagina (wrapper) */
static inline uint64_t mm_page_size(void) {
  return arch_memory_page_size();
}

/* Allineamento e test */
static inline bool mm_is_page_aligned(uint64_t addr) {
  uint64_t ps = mm_page_size();
  return (addr & (ps - 1)) == 0;
}

static inline uint64_t mm_page_align_down(uint64_t addr) {
  uint64_t ps = mm_page_size();
  return addr & ~(ps - 1);
}

static inline uint64_t mm_page_align_up(uint64_t addr) {
  uint64_t ps = mm_page_size();
  return (addr + (ps - 1)) & ~(ps - 1);
}

/* Conversioni */
static inline uint64_t mm_addr_to_page(uint64_t addr) {
  return addr / mm_page_size();
}

static inline uint64_t mm_page_to_addr(uint64_t page) {
  return page * mm_page_size();
}
