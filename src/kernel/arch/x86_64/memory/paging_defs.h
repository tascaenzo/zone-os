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
#define PAGE_SHIFT 12

// Maschera per isolare l'offset all'interno della pagina
#define PAGE_MASK 0xFFFUL

/* -------------------------------------------------------------------------- */
/*                            Macro di utilità                                */
/* -------------------------------------------------------------------------- */

// Allineamento verso il basso all'inizio della pagina
#define PAGE_ALIGN_DOWN(addr) ((u64)(addr) & ~PAGE_MASK)

// Allineamento verso l'alto all'inizio della prossima pagina
#define PAGE_ALIGN_UP(addr) (((u64)(addr) + PAGE_MASK) & ~PAGE_MASK)

// Conversione indirizzo → indice pagina
#define ADDR_TO_PAGE(addr) ((u64)(addr) >> PAGE_SHIFT)

// Conversione indice pagina → indirizzo
#define PAGE_TO_ADDR(page) ((u64)(page) << PAGE_SHIFT)

// Verifica se un indirizzo è allineato a pagina
#define IS_PAGE_ALIGNED(addr) (((u64)(addr) & PAGE_MASK) == 0)

/* -------------------------------------------------------------------------- */
/*                      Taglia standard per calcoli memoria                   */
/* -------------------------------------------------------------------------- */

#define KB 1024UL
#define MB (1024UL * KB)
#define GB (1024UL * MB)