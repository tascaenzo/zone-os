#pragma once
#include <lib/types.h>

/**
 * @file arch/x86_64/paging.h
 * @brief Costanti e strutture di paging specifiche per x86_64
 */

// Costanti di base per le pagine x86_64
#define PAGE_SIZE 4096  // 4KB per pagina
#define PAGE_SHIFT 12   // LOG2(PAGE_SIZE)
#define PAGE_MASK 0xFFF // Maschera per offset nella pagina

// Macro di utilità per conversioni indirizzo/pagina
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~PAGE_MASK)
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_MASK) & ~PAGE_MASK)
#define ADDR_TO_PAGE(addr) ((addr) >> PAGE_SHIFT)
#define PAGE_TO_ADDR(page) ((page) << PAGE_SHIFT)
#define IS_PAGE_ALIGNED(addr) (((addr) & PAGE_MASK) == 0)

// Costanti per page table hierarchy x86_64
#define ENTRIES_PER_TABLE 512 // Entries per page table
#define TABLE_SHIFT 9         // LOG2(ENTRIES_PER_TABLE)
#define TABLE_MASK 0x1FF      // Maschera per entry in table

// Livelli della page table hierarchy
#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT 21
#define PT_SHIFT 12

// Macro per estrarre indici dalle tabelle
#define PML4_INDEX(addr) (((addr) >> PML4_SHIFT) & TABLE_MASK)
#define PDPT_INDEX(addr) (((addr) >> PDPT_SHIFT) & TABLE_MASK)
#define PD_INDEX(addr) (((addr) >> PD_SHIFT) & TABLE_MASK)
#define PT_INDEX(addr) (((addr) >> PT_SHIFT) & TABLE_MASK)

// Layout di memoria virtuale x86_64 (canonical addressing)
#define KERNEL_VIRTUAL_BASE 0xFFFF800000000000ULL
#define USER_VIRTUAL_MAX 0x00007FFFFFFFFFFFULL
#define HIGHER_HALF_OFFSET KERNEL_VIRTUAL_BASE

// Dimensioni comuni
#define KB 1024ULL
#define MB (1024ULL * KB)
#define GB (1024ULL * MB)
#define TB (1024ULL * GB)

// Flags per page table entries (per futuro VMM)
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_WRITETHROUGH (1ULL << 3)
#define PAGE_CACHE_DISABLE (1ULL << 4)
#define PAGE_ACCESSED (1ULL << 5)
#define PAGE_DIRTY (1ULL << 6)
#define PAGE_HUGE (1ULL << 7)
#define PAGE_GLOBAL (1ULL << 8)
#define PAGE_NO_EXECUTE (1ULL << 63)

// Struttura di una page table entry x86_64 (per futuro VMM)
typedef union {
  struct {
    u64 present : 1;
    u64 writable : 1;
    u64 user : 1;
    u64 writethrough : 1;
    u64 cache_disable : 1;
    u64 accessed : 1;
    u64 dirty : 1;
    u64 huge : 1;
    u64 global : 1;
    u64 available1 : 3;
    u64 address : 40; // Physical address bits [51:12]
    u64 available2 : 11;
    u64 no_execute : 1;
  } __attribute__((packed));
  u64 raw;
} page_table_entry_t;