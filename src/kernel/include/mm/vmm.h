#pragma once

#include <arch/memory.h>
#include <lib/types.h>

/* -------------------------------------------------------------------------- */
/*                         VMM CONFIGURATION CONSTANTS                        */
/* -------------------------------------------------------------------------- */

/*
 * Numero massimo di pagine che consideriamo "ragionevole" per una singola
 * operazione di mapping. Oltre questo limite viene emesso un warning in fase
 * di debug in quanto l'operazione potrebbe richiedere molto tempo (4GB).
 */
#define VMM_MAX_MAPPING_PAGES (1UL << 20)

/**
 * @file mm/vmm.h
 * @brief Virtual Memory Manager - Architecture Agnostic
 *
 * Il VMM gestisce la memoria virtuale per il microkernel e gli utenti.
 * Astrae le page table arch-specifiche tramite funzioni modulari.
 *
 * DESIGN PRINCIPLES:
 * - Separazione completa tra logica e struttura delle page table
 * - Interfaccia unificata per la gestione mapping, unmapping, e traduzione
 * - Supporto a più spazi di indirizzamento (kernel/user)
 * - Estensibile a più architetture via arch-specific backends
 *
 * DEPENDENCIES:
 * - arch/memory.h: definizioni PAGE_SIZE, flag e utilità paging
 * - arch/vmm_defs.h: (separato) per struttura interna page tables
 *
 * ┌────────────────────────────────────────────┐
 * │         Mappa dello Spazio Virtuale        │
 * └────────────────────────────────────────────┘
 *
 *  +-------------------------------+ ← 0xFFFF_FFFF_FFFF_FFFF (64-bit top)
 *  |        Kernel space           |
 *  |                               |
 *  |  ┌─────────────────────────┐  |
 *  |  │ Kernel text (.text)     │ → codice del kernel (readonly, NX)
 *  |  ├─────────────────────────┤  |
 *  |  │ Kernel data (.data/.bss)│ → variabili globali e statiche
 *  |  ├─────────────────────────┤  |
 *  |  │ Kernel heap             │ → allocazioni dinamiche nel kernel
 *  |  ├─────────────────────────┤  |
 *  |  │ Mappature fisiche       │ → dispositivi, framebuffer, APIC, etc.
 *  |  └─────────────────────────┘  |
 *  |                               |
 *  +-------------------------------+
 *  |      [GAP non mappato]        | ← protezione da null-deref/kernel ptr
 *  +-------------------------------+
 *  |         User space            |
 *  |                               |
 *  |  ┌─────────────────────────┐  |
 *  |  │ Process text segment    │ → codice utente (readonly)
 *  |  ├─────────────────────────┤  |
 *  |  │ Process data segment    │ → variabili globali dell'app
 *  |  ├─────────────────────────┤  |
 *  |  │ Heap (malloc/sbrk)      │ → memoria dinamica dell'app
 *  |  ├─────────────────────────┤  |
 *  |  │ Stack                   │ → stack utente (crescita ↓)
 *  |  └─────────────────────────┘  |
 *  |                               |
 *  +-------------------------------+ ← 0x0000_0000_0000_0000 (virtual base)
 *
 * NOTE:
 * - Le mappature fisiche sono accessibili solo dal kernel
 * - Il VMM imposta protezioni NX, R/W, e user/kernel bit
 * - Il kernel usa pagine huge (2M) ove possibile per performance
 *
 */

/*
 * ============================================================================
 * TYPES
 * ============================================================================
 */

/**
 * @brief Flag per mappature virtuali
 */
typedef enum {
  VMM_FLAG_READ = (1 << 0),
  VMM_FLAG_WRITE = (1 << 1),
  VMM_FLAG_EXEC = (1 << 2),
  VMM_FLAG_USER = (1 << 3),
  VMM_FLAG_GLOBAL = (1 << 4),
  VMM_FLAG_NO_CACHE = (1 << 5),
} vmm_flags_t;

/**
 * @brief Tipo astratto per uno spazio di indirizzamento
 */
typedef struct vmm_space vmm_space_t;

/*
 * ============================================================================
 * INITIALIZATION
 * ============================================================================
 */

/**
 * @brief Inizializza il VMM globale del microkernel
 *
 * - Imposta lo spazio di indirizzamento kernel
 * - Prepara la struttura per gestire mapping dinamici
 * - Abilita paging sull’architettura corrente
 */
void vmm_init(void);

/*
 * ============================================================================
 * KERNEL ADDRESS SPACE
 * ============================================================================
 */

/**
 * @brief Ottiene lo spazio di indirizzamento del kernel
 *
 * @return Puntatore statico allo spazio kernel
 */
vmm_space_t *vmm_kernel_space(void);

/*
 * ============================================================================
 * ADDRESS SPACE MANAGEMENT
 * ============================================================================
 */

/**
 * @brief Crea un nuovo spazio virtuale (es. per processo)
 *
 * @return Puntatore a nuova vmm_space_t, o NULL su errore
 */
vmm_space_t *vmm_create_space(void);

/**
 * @brief Distrugge uno spazio virtuale e libera risorse
 *
 * @param space Puntatore allo spazio da distruggere
 */
void vmm_destroy_space(vmm_space_t *space);

/**
 * @brief Attiva uno spazio virtuale come corrente (CR3 switch)
 *
 * @param space Spazio da attivare (es. processo o kernel)
 */
void vmm_switch_space(vmm_space_t *space);

/*
 * ============================================================================
 * MAPPING / UNMAPPING API
 * ============================================================================
 */

/**
 * @brief Mappa un range fisico nello spazio virtuale
 *
 * @param space Spazio target (NULL = spazio corrente)
 * @param virt_addr Indirizzo virtuale base (allineato a pagina)
 * @param phys_addr Indirizzo fisico base (allineato a pagina)
 * @param page_count Numero di pagine da mappare
 * @param flags Combinazione di VMM_FLAG_*
 * @return true se la mappatura è andata a buon fine
 */
bool vmm_map(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count, u64 flags);

/**
 * @brief Annulla la mappatura virtuale di un range
 *
 * @param space Spazio target (NULL = spazio corrente)
 * @param virt_addr Indirizzo virtuale base
 * @param page_count Numero di pagine da smappare
 */
void vmm_unmap(vmm_space_t *space, u64 virt_addr, size_t page_count);

/**
 * @brief Ottiene la mappatura fisica associata a un indirizzo virtuale
 *
 * @param space Spazio target
 * @param virt_addr Indirizzo virtuale da tradurre
 * @param out_phys_addr (opzionale) dove salvare l’indirizzo fisico
 * @return true se esiste una mappatura valida
 */
bool vmm_resolve(vmm_space_t *space, u64 virt_addr, u64 *out_phys_addr);

/*
 * ============================================================================
 * DEBUG AND INTROSPECTION
 * ============================================================================
 */

/**
 * @brief Stampa lo stato delle page table per uno spazio
 *
 * @param space Spazio da ispezionare
 */
void vmm_debug_dump(vmm_space_t *space);

/**
 * @brief Verifica che lo spazio abbia tutte le pagine mappate valide
 *
 * @param space Spazio da validare
 * @return true se integro
 */
bool vmm_check_integrity(vmm_space_t *space);

/**
 * @brief Converte un indirizzo fisico in uno virtuale (solo per il kernel)
 *
 * @param phys_addr Indirizzo fisico
 * @return Indirizzo virtuale corrispondente
 */
void *vmm_phys_to_virt(u64 phys_addr);

/**
 * @brief Converte un indirizzo virtuale in fisico
 *
 * @param virt_addr Indirizzo virtuale
 * @return Indirizzo fisico corrispondente
 */
u64 vmm_virt_to_phys(u64 virt_addr);
