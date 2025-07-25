#pragma once

#include <arch/memory.h> // Per PAGE_SIZE, IS_PAGE_ALIGNED, etc.
#include <lib/types.h>

/**
 * @file arch/x86_64/vmm_defs.h
 * @brief Definizioni specifiche x86_64 per Virtual Memory Management
 *
 * Contiene tutte le strutture, costanti e macro necessarie per gestire
 * le page table x86_64. Questo file è specifico dell'architettura e
 * non dovrebbe essere incluso direttamente dal codice generico.
 *
 * GERARCHIA PAGE TABLE x86_64 (4-level paging):
 *
 * Virtual Address (64-bit):
 * ┌─────┬─────┬─────┬─────┬─────┬──────────────┐
 * │Sign │PML4 │PDPT │PD   │PT   │Page Offset   │
 * │Ext  │     │     │     │     │              │
 * └─────┴─────┴─────┴─────┴─────┴──────────────┘
 *  63-48 47-39 38-30 29-21 20-12 11-0
 *
 * Struttura delle page table:
 * PML4 (Page Map Level 4) → PDPT (Page Directory Pointer Table)
 *   → PD (Page Directory) → PT (Page Table) → Physical Page
 */

/*
 * ============================================================================
 * VIRTUAL ADDRESS LAYOUT x86_64
 * ============================================================================
 */

// Indici dei bit nella virtual address
#define VMM_X86_64_PAGE_OFFSET_BITS 12 // Bit 11-0: offset nella pagina
#define VMM_X86_64_PAGE_TABLE_BITS 9   // Bit 20-12: indice page table
#define VMM_X86_64_PAGE_DIR_BITS 9     // Bit 29-21: indice page directory
#define VMM_X86_64_PDPT_BITS 9         // Bit 38-30: indice PDPT
#define VMM_X86_64_PML4_BITS 9         // Bit 47-39: indice PML4
#define VMM_X86_64_SIGN_EXT_BITS 16    // Bit 63-48: sign extension

// Shift values per estrarre indici
#define VMM_X86_64_PAGE_OFFSET_SHIFT 0
#define VMM_X86_64_PAGE_TABLE_SHIFT 12
#define VMM_X86_64_PAGE_DIR_SHIFT 21
#define VMM_X86_64_PDPT_SHIFT 30
#define VMM_X86_64_PML4_SHIFT 39

// Maschere per estrarre indici (9 bit = 0x1FF)
#define VMM_X86_64_INDEX_MASK 0x1FF

// Numero di entries per livello (2^9 = 512)
#define VMM_X86_64_ENTRIES_PER_TABLE 512

/*
 * ============================================================================
 * PAGE TABLE ENTRY FLAGS x86_64
 * ============================================================================
 */

// Bit standard delle page table entry
#define VMM_X86_64_PRESENT (1UL << 0)       // Pagina presente in memoria
#define VMM_X86_64_WRITABLE (1UL << 1)      // Pagina scrivibile
#define VMM_X86_64_USER (1UL << 2)          // Accessibile da user mode
#define VMM_X86_64_WRITE_THROUGH (1UL << 3) // Write-through caching
#define VMM_X86_64_CACHE_DISABLE (1UL << 4) // Disabilita cache
#define VMM_X86_64_ACCESSED (1UL << 5)      // Pagina acceduta (set da CPU)
#define VMM_X86_64_DIRTY (1UL << 6)         // Pagina modificata (set da CPU)
#define VMM_X86_64_PAGE_SIZE (1UL << 7)     // Huge page (2MB/1GB)
#define VMM_X86_64_GLOBAL (1UL << 8)        // Pagina globale (non flush TLB)
#define VMM_X86_64_NO_EXECUTE (1UL << 63)   // Non eseguibile (richiede NX bit)

// Bit disponibili per uso OS (9-11)
#define VMM_X86_64_OS_BIT_0 (1UL << 9)
#define VMM_X86_64_OS_BIT_1 (1UL << 10)
#define VMM_X86_64_OS_BIT_2 (1UL << 11)

// Maschera per indirizzo fisico nella PTE (bit 51-12)
#define VMM_X86_64_PHYS_ADDR_MASK 0x000FFFFFFFFFF000UL

/*
 * ============================================================================
 * MEMORY LAYOUT CONSTANTS
 * ============================================================================
 */

// Layout dello spazio virtuale x86_64
#define VMM_X86_64_KERNEL_BASE 0xFFFF800000000000UL // -128TB (kernel space)
#define VMM_X86_64_USER_MAX 0x00007FFFFFFFFFFFFUL   // 128TB-1 (user space max)

// Specific kernel regions
#define VMM_X86_64_KERNEL_TEXT 0xFFFFFFFF80000000UL // -2GB (kernel text)
#define VMM_X86_64_KERNEL_HEAP 0xFFFF888000000000UL // -64TB (kernel heap)
#define VMM_X86_64_DIRECT_MAP 0xFFFF888000000000UL  // Physical memory direct map

// Helpers per conversione indirizzi fisici ↔ virtuali nel direct map
#define VMM_X86_64_PHYS_TO_VIRT(addr)                                             \
  ((void *)((u64)(addr) + VMM_X86_64_DIRECT_MAP))
#define VMM_X86_64_VIRT_TO_PHYS(addr)                                             \
  ((u64)(addr) - VMM_X86_64_DIRECT_MAP)

// User space layout
#define VMM_X86_64_USER_BASE 0x0000000000400000UL  // 4MB (user start)
#define VMM_X86_64_USER_STACK 0x0000700000000000UL // 112TB (user stack)

/*
 * ============================================================================
 * PAGE TABLE STRUCTURES
 * ============================================================================
 */

/**
 * @brief Page Table Entry (64-bit)
 *
 * Rappresenta una singola entry nelle page table x86_64.
 * Contiene l'indirizzo fisico e i flag di controllo.
 */
typedef union {
  u64 raw; // Accesso raw a 64-bit

  struct {
    u64 present : 1;       // Bit 0: Present
    u64 writable : 1;      // Bit 1: Read/Write
    u64 user : 1;          // Bit 2: User/Supervisor
    u64 write_through : 1; // Bit 3: Write-Through
    u64 cache_disable : 1; // Bit 4: Cache Disable
    u64 accessed : 1;      // Bit 5: Accessed
    u64 dirty : 1;         // Bit 6: Dirty
    u64 page_size : 1;     // Bit 7: Page Size (huge pages)
    u64 global : 1;        // Bit 8: Global
    u64 os_bits : 3;       // Bit 9-11: Available for OS
    u64 phys_addr : 40;    // Bit 12-51: Physical Address
    u64 reserved : 11;     // Bit 52-62: Reserved
    u64 no_execute : 1;    // Bit 63: No Execute
  } __attribute__((packed));
} vmm_x86_64_pte_t;

/**
 * @brief Page Table (512 entries da 8 byte = 4KB)
 *
 * Una singola page table contiene 512 entries, ognuna da 64-bit.
 * Deve essere allineata a 4KB boundary.
 */
typedef struct {
  vmm_x86_64_pte_t entries[VMM_X86_64_ENTRIES_PER_TABLE];
} __attribute__((aligned(PAGE_SIZE))) vmm_x86_64_page_table_t;

/**
 * @brief Struttura completa di un address space x86_64
 *
 * Contiene il puntatore alla PML4 (root table) e metadati
 * per gestire lo spazio di indirizzamento.
 */
typedef struct {
  vmm_x86_64_page_table_t *pml4; // Root page table (livello 4)
  u64 phys_pml4;                 // Indirizzo fisico della PML4 (per CR3)
  u64 mapped_pages;              // Numero di pagine mappate
  bool is_kernel_space;          // True se è lo spazio kernel
} vmm_x86_64_space_t;

/*
 * ============================================================================
 * UTILITY MACROS
 * ============================================================================
 */

/**
 * @brief Estrae l'indice PML4 da un indirizzo virtuale
 */
#define VMM_X86_64_PML4_INDEX(vaddr) (((vaddr) >> VMM_X86_64_PML4_SHIFT) & VMM_X86_64_INDEX_MASK)

/**
 * @brief Estrae l'indice PDPT da un indirizzo virtuale
 */
#define VMM_X86_64_PDPT_INDEX(vaddr) (((vaddr) >> VMM_X86_64_PDPT_SHIFT) & VMM_X86_64_INDEX_MASK)

/**
 * @brief Estrae l'indice Page Directory da un indirizzo virtuale
 */
#define VMM_X86_64_PD_INDEX(vaddr) (((vaddr) >> VMM_X86_64_PAGE_DIR_SHIFT) & VMM_X86_64_INDEX_MASK)

/**
 * @brief Estrae l'indice Page Table da un indirizzo virtuale
 */
#define VMM_X86_64_PT_INDEX(vaddr) (((vaddr) >> VMM_X86_64_PAGE_TABLE_SHIFT) & VMM_X86_64_INDEX_MASK)

/**
 * @brief Estrae l'offset nella pagina da un indirizzo virtuale
 */
#define VMM_X86_64_PAGE_OFFSET(vaddr) ((vaddr) & (PAGE_SIZE - 1))

/**
 * @brief Estrae l'indirizzo fisico da una PTE
 */
#define VMM_X86_64_PTE_ADDR(pte) ((pte) & VMM_X86_64_PHYS_ADDR_MASK)

/**
 * @brief Crea una PTE con indirizzo fisico e flag
 */
#define VMM_X86_64_MAKE_PTE(phys_addr, flags) (((phys_addr) & VMM_X86_64_PHYS_ADDR_MASK) | (flags))

/**
 * @brief Verifica se una PTE è presente
 */
#define VMM_X86_64_PTE_PRESENT(pte) ((pte) & VMM_X86_64_PRESENT)

/**
 * @brief Canonicalizza un indirizzo virtuale x86_64
 *
 * x86_64 richiede che i bit 63-48 siano tutti uguali al bit 47
 * (sign extension). Questa macro forza la canonicalizzazione.
 */
#define VMM_X86_64_CANONICALIZE(vaddr) ((((vaddr) & (1UL << 47)) != 0) ? ((vaddr) | 0xFFFF000000000000UL) : ((vaddr) & 0x0000FFFFFFFFFFFFFUL))

/**
 * @brief Verifica se un indirizzo virtuale è canonico
 */
#define VMM_X86_64_IS_CANONICAL(vaddr) (((vaddr) == VMM_X86_64_CANONICALIZE(vaddr)))

/*
 * ============================================================================
 * CONVERSION MACROS
 * ============================================================================
 */

// Forward declaration per evitare include circolari
typedef enum {
  VMM_FLAG_READ = (1 << 0),
  VMM_FLAG_WRITE = (1 << 1),
  VMM_FLAG_EXEC = (1 << 2),
  VMM_FLAG_USER = (1 << 3),
  VMM_FLAG_GLOBAL = (1 << 4),
  VMM_FLAG_NO_CACHE = (1 << 5),
} vmm_flags_t;

/**
 * @brief Converte flag VMM generici in flag x86_64 specifici
 */
static inline u64 vmm_x86_64_convert_flags(u64 generic_flags) {
  u64 x86_flags = VMM_X86_64_PRESENT; // Sempre presente se stiamo mappando

  if (generic_flags & VMM_FLAG_WRITE)
    x86_flags |= VMM_X86_64_WRITABLE;
  if (generic_flags & VMM_FLAG_USER)
    x86_flags |= VMM_X86_64_USER;
  if (generic_flags & VMM_FLAG_GLOBAL)
    x86_flags |= VMM_X86_64_GLOBAL;
  if (generic_flags & VMM_FLAG_NO_CACHE)
    x86_flags |= VMM_X86_64_CACHE_DISABLE;
  if (!(generic_flags & VMM_FLAG_EXEC))
    x86_flags |= VMM_X86_64_NO_EXECUTE;

  return x86_flags;
}

/*
 * ============================================================================
 * CONSTANTS FOR DEBUGGING
 * ============================================================================
 */

// Nomi human-readable per debug
#define VMM_X86_64_LEVEL_NAMES {"PT", "PD", "PDPT", "PML4"}
#define VMM_X86_64_MAX_LEVELS 4

// Dimensioni delle regioni per ogni livello
#define VMM_X86_64_PT_SIZE (PAGE_SIZE)                    // 4KB
#define VMM_X86_64_PD_SIZE (VMM_X86_64_PT_SIZE * 512)     // 2MB
#define VMM_X86_64_PDPT_SIZE (VMM_X86_64_PD_SIZE * 512)   // 1GB
#define VMM_X86_64_PML4_SIZE (VMM_X86_64_PDPT_SIZE * 512) // 512GB

/*
 * ============================================================================
 * FORWARD DECLARATIONS AND OPAQUE TYPES
 * ============================================================================
 */

/**
 * @brief Tipo opaco per uno spazio di indirizzamento
 *
 * La definizione completa è in vmm_arch.c, qui facciamo solo forward declaration.
 * Questo permette di usare vmm_space_t* negli header senza esporre l'implementazione.
 */
typedef struct vmm_space vmm_space_t;

/*
 * ============================================================================
 * FORWARD DECLARATIONS - IMPLEMENTATE IN vmm_arch.c
 * ============================================================================
 */

/**
 * @brief Inizializza il paging x86_64 e abilita le funzionalità necessarie
 */
void vmm_x86_64_init_paging(void);

/**
 * @brief Abilita il bit NX (No-Execute) nel registro EFER
 */
void vmm_x86_64_enable_nx(void);

/**
 * @brief Crea un nuovo spazio di indirizzamento x86_64
 */
vmm_space_t *vmm_x86_64_create_space(void);

/**
 * @brief Distrugge uno spazio di indirizzamento x86_64
 */
void vmm_x86_64_destroy_space(vmm_space_t *space);

/**
 * @brief Attiva uno spazio di indirizzamento (carica CR3)
 */
void vmm_x86_64_switch_space(vmm_space_t *space);

/**
 * @brief Implementazione arch-specific del mapping
 */
bool vmm_x86_64_map_pages(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count, u64 flags);

/**
 * @brief Implementazione arch-specific dell'unmapping
 */
void vmm_x86_64_unmap_pages(vmm_space_t *space, u64 virt_addr, size_t page_count);

/**
 * @brief Risoluzione indirizzo virtuale → fisico
 */
bool vmm_x86_64_resolve(vmm_space_t *space, u64 virt_addr, u64 *phys_addr);

/**
 * @brief Debug dump delle page table
 */
void vmm_x86_64_debug_dump(vmm_space_t *space);

/**
 * @brief Ritorna lo spazio kernel
 */
vmm_space_t *vmm_x86_64_get_kernel_space(void);

/*
 * ============================================================================
 * SIMPLE INLINE ASSEMBLY HELPERS (SAFE FOR HEADERS)
 * ============================================================================
 */

/**
 * @brief Legge il registro CR3 (Page Directory Base)
 */
static inline u64 vmm_x86_64_read_cr3(void) {
  u64 cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

/**
 * @brief Scrive il registro CR3 (Page Directory Base)
 */
static inline void vmm_x86_64_write_cr3(u64 cr3) {
  __asm__ volatile("mov %0, %%cr3" ::"r"(cr3) : "memory");
}

/**
 * @brief Invalida una singola pagina nel TLB
 */
static inline void vmm_x86_64_invlpg(u64 vaddr) {
  __asm__ volatile("invlpg (%0)" ::"r"(vaddr) : "memory");
}

/**
 * @brief Flush completo del TLB
 */
static inline void vmm_x86_64_flush_tlb(void) {
  u64 cr3 = vmm_x86_64_read_cr3();
  vmm_x86_64_write_cr3(cr3); // Reload CR3 per flush TLB
}