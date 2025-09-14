#pragma once
#include <lib/types.h>

/**
 * @file arch/x86_64/pmm_defs.h
 * @brief Definizioni architetturali per il PMM (x86_64)
 *
 * Questo file contiene solo le costanti e macro necessarie
 * per la gestione della memoria fisica a livello di PMM.
 *
 * NON include strutture o definizioni di paging virtuale.
 */

/* -------------------------------------------------------------------------- */
/*                        Costanti base della memoria                         */
/* -------------------------------------------------------------------------- */

// Dimensione di una pagina fisica (4KB standard su x86_64)
#define PAGE_SIZE 4096UL

// Bit shift per calcolare dimensione pagina (2^12 = 4096)
#define ARCH_PAGE_SHIFT 12

// Maschera per isolare l'offset all'interno della pagina
#define PAGE_MASK 0xFFFUL

int arch_page_size(void);