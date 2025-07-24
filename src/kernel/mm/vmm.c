#include <klib/klog.h>
#include <lib/string.h>
#include <lib/types.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

/**
 * @file mm/vmm.c
 * @brief Virtual Memory Manager - Implementazione architecture-agnostic
 *
 * Questo file implementa SOLO la logica comune del VMM che è indipendente
 * dall'architettura. Le funzioni arch-specific sono implementate in arch/<arch>/vmm_arch.c
 * e dichiarate come external.
 *
 * DESIGN PRINCIPLES:
 * - Questo file NON include mai header arch-specific
 * - Le funzioni arch-specific sono chiamate tramite external declarations
 * - La selezione dell'architettura avviene a link-time, non compile-time
 * - Tutta la logica comune (validazioni, statistiche, logging) è gestita qui
 *
 * PATTERN:
 * 1. Validazione parametri comuni
 * 2. Logging operazioni
 * 3. Chiamata a funzione arch-specific (external)
 * 4. Gestione del risultato e aggiornamento statistiche
 */

/*
 * =========================================================================
 * EXTERNAL DECLARATIONS - IMPLEMENTATE IN arch/<arch>/vmm_arch.c
 * =========================================================================
 */

// Funzioni arch-specific - implementate in arch/*/vmm_arch.c
extern void arch_vmm_init(void);
extern vmm_space_t *arch_vmm_get_kernel_space(void);
extern vmm_space_t *arch_vmm_create_space(void);
extern void arch_vmm_destroy_space(vmm_space_t *space);
extern void arch_vmm_switch_space(vmm_space_t *space);
extern bool arch_vmm_map_pages(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count, u64 flags);
extern void arch_vmm_unmap_pages(vmm_space_t *space, u64 virt_addr, size_t page_count);
extern bool arch_vmm_resolve(vmm_space_t *space, u64 virt_addr, u64 *phys_addr);
extern void arch_vmm_debug_dump(vmm_space_t *space);
extern bool arch_vmm_check_integrity(vmm_space_t *space);

/*
 * ============================================================================
 * STATO GLOBALE DEL VMM
 * ============================================================================
 */

/**
 * @brief Stato interno del VMM generico
 */
static struct {
  bool initialized;          // VMM è stato inizializzato?
  vmm_space_t *kernel_space; // Puntatore allo spazio kernel
  u64 total_spaces_created;  // Statistiche globali
  u64 total_mappings;        // Numero totale di mapping eseguiti
  u64 total_unmappings;      // Numero totale di unmapping eseguiti
} vmm_state = {.initialized = false, .kernel_space = 0, .total_spaces_created = 0, .total_mappings = 0, .total_unmappings = 0};

/*
 * ============================================================================
 * UTILITY FUNCTIONS - ARCHITECTURE INDEPENDENT
 * ============================================================================
 */

/**
 * @brief Valida i parametri comuni per operazioni di mapping
 */
static bool validate_mapping_params(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count) {
  if (!space) {
    klog_error("VMM: Spazio NULL passato a validate_mapping_params");
    return false;
  }

  // Verifica che VMM sia inizializzato
  if (!vmm_state.initialized) {
    klog_error("VMM: Tentativo di mapping su VMM non inizializzato");
    return false;
  }

  // Verifica allineamento degli indirizzi (usa macro da arch/memory.h)
  if (!IS_PAGE_ALIGNED(virt_addr)) {
    klog_error("VMM: Indirizzo virtuale non allineato: 0x%lx", virt_addr);
    return false;
  }

  if (!IS_PAGE_ALIGNED(phys_addr)) {
    klog_error("VMM: Indirizzo fisico non allineato: 0x%lx", phys_addr);
    return false;
  }

  // Verifica che page_count sia sensato
  if (page_count == 0) {
    klog_error("VMM: Tentativo di mappare 0 pagine");
    return false;
  }

  if (page_count > VMM_MAX_MAPPING_PAGES) { // Limite ragionevole: 4GB
    klog_warn("VMM: Richiesta mapping molto grande: %zu pagine (%zu MB)", page_count, (page_count * PAGE_SIZE) / (1024 * 1024));
  }

  return true;
}

/**
 * @brief Valida i parametri per operazioni su spazi di indirizzamento
 */
static bool validate_space_operation(vmm_space_t *space, const char *operation) {
  if (!vmm_state.initialized) {
    klog_error("VMM: %s su VMM non inizializzato", operation);
    return false;
  }

  if (!space) {
    klog_error("VMM: %s su spazio NULL", operation);
    return false;
  }

  return true;
}

/*
 * ============================================================================
 * IMPLEMENTAZIONE API PUBBLICA
 * ============================================================================
 */

/**
 * @brief Inizializza il Virtual Memory Manager
 */
void vmm_init(void) {
  klog_info("VMM: Inizializzazione Virtual Memory Manager");

  // Verifica che il PMM sia già inizializzato
  const pmm_stats_t *pmm_stats = pmm_get_stats();
  if (!pmm_stats) {
    klog_panic("VMM: PMM deve essere inizializzato prima del VMM");
  }

  klog_info("VMM: PMM verificato - %lu MB disponibili", pmm_stats->free_pages * PAGE_SIZE / (1024 * 1024));

  // Delega l'inizializzazione all'arch layer
  arch_vmm_init();

  // Ottieni il kernel space dall'arch layer
  vmm_state.kernel_space = arch_vmm_get_kernel_space();
  if (!vmm_state.kernel_space) {
    klog_panic("VMM: Impossibile ottenere kernel space dall'arch layer");
  }

  // Inizializza statistiche
  vmm_state.total_spaces_created = 1; // Kernel space
  vmm_state.total_mappings = 0;
  vmm_state.total_unmappings = 0;
  vmm_state.initialized = true;

  klog_info("VMM: Inizializzazione completata con successo");
  klog_info("VMM: Kernel space: %p", vmm_state.kernel_space);
}

/**
 * @brief Ottiene lo spazio di indirizzamento del kernel
 */
vmm_space_t *vmm_kernel_space(void) {
  if (!vmm_state.initialized) {
    klog_error("VMM: Richiesta kernel space su VMM non inizializzato");
    return (vmm_space_t *)NULL;
  }

  return vmm_state.kernel_space;
}

/**
 * @brief Crea un nuovo spazio virtuale
 */
vmm_space_t *vmm_create_space(void) {
  if (!vmm_state.initialized) {
    klog_error("VMM: Tentativo di creare spazio su VMM non inizializzato");
    return NULL;
  }

  klog_debug("VMM: Creazione nuovo spazio di indirizzamento");

  // Delega all'implementazione arch-specific
  vmm_space_t *new_space = arch_vmm_create_space();

  if (new_space) {
    vmm_state.total_spaces_created++;
    klog_debug("VMM: Nuovo spazio creato con successo (%p)", new_space);
  } else {
    klog_error("VMM: Fallita creazione nuovo spazio");
  }

  return new_space;
}

/**
 * @brief Distrugge uno spazio virtuale
 */
void vmm_destroy_space(vmm_space_t *space) {
  if (!validate_space_operation(space, "distruzione spazio")) {
    return;
  }

  // Non permettere distruzione del kernel space
  if (space == vmm_state.kernel_space) {
    klog_error("VMM: Tentativo di distruggere kernel space!");
    return;
  }

  klog_debug("VMM: Distruzione spazio %p", space);

  // Delega all'implementazione arch-specific
  arch_vmm_destroy_space(space);

  klog_debug("VMM: Spazio distrutto con successo");
}

/**
 * @brief Attiva uno spazio virtuale come corrente
 */
void vmm_switch_space(vmm_space_t *space) {
  if (!validate_space_operation(space, "switch spazio")) {
    return;
  }

  klog_debug("VMM: Switch a spazio %p", space);

  // Delega all'implementazione arch-specific
  arch_vmm_switch_space(space);

  klog_debug("VMM: Switch completato");
}

/**
 * @brief Mappa un range fisico nello spazio virtuale
 */
bool vmm_map(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count, u64 flags) {

  // Se space è NULL, usa kernel space
  if (!space) {
    space = vmm_state.kernel_space;
  }

  // Validazione parametri
  if (!validate_mapping_params(space, virt_addr, phys_addr, page_count)) {
    return false;
  }

  // Validazione flag
  if (flags == 0) {
    klog_warn("VMM: Mapping con flag=0, aggiungendo READ di default");
    flags = VMM_FLAG_READ;
  }

  klog_debug("VMM: Mapping %zu pagine: 0x%lx→0x%lx (flags=0x%lx)", page_count, virt_addr, phys_addr, flags);

  // Delega all'implementazione arch-specific
  bool success = arch_vmm_map_pages(space, virt_addr, phys_addr, page_count, flags);

  if (success) {
    vmm_state.total_mappings += page_count;
    klog_debug("VMM: Mapping completato con successo");
  } else {
    klog_error("VMM: Mapping fallito");
  }

  return success;
}

/**
 * @brief Annulla la mappatura virtuale di un range
 */
void vmm_unmap(vmm_space_t *space, u64 virt_addr, size_t page_count) {
  // Se space è NULL, usa kernel space
  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation(space, "unmapping")) {
    return;
  }

  if (!IS_PAGE_ALIGNED(virt_addr)) {
    klog_error("VMM: Indirizzo virtuale per unmap non allineato: 0x%lx", virt_addr);
    return;
  }

  if (page_count == 0) {
    klog_error("VMM: Tentativo di unmappare 0 pagine");
    return;
  }

  klog_debug("VMM: Unmapping %zu pagine da 0x%lx", page_count, virt_addr);

  // Delega all'implementazione arch-specific
  arch_vmm_unmap_pages(space, virt_addr, page_count);

  vmm_state.total_unmappings += page_count;
  klog_debug("VMM: Unmapping completato");
}

/**
 * @brief Ottiene la mappatura fisica di un indirizzo virtuale
 */
bool vmm_resolve(vmm_space_t *space, u64 virt_addr, u64 *out_phys_addr) {
  // Se space è NULL, usa kernel space
  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation(space, "resolve")) {
    return false;
  }

  // Delega all'implementazione arch-specific
  bool found = arch_vmm_resolve(space, virt_addr, out_phys_addr);

  if (found && out_phys_addr) {
    klog_debug("VMM: Resolve 0x%lx → 0x%lx", virt_addr, *out_phys_addr);
  } else {
    klog_debug("VMM: Resolve 0x%lx → non mappato", virt_addr);
  }

  return found;
}

/**
 * @brief Stampa lo stato delle page table per uno spazio
 */
void vmm_debug_dump(vmm_space_t *space) {
  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation(space, "debug dump")) {
    return;
  }

  klog_info("=== VMM DEBUG DUMP ===");
  klog_info("Spazio: %p", space);
  klog_info("Kernel space: %s", (space == vmm_state.kernel_space) ? "Si" : "No");

  // Delega all'implementazione arch-specific
  arch_vmm_debug_dump(space);

  klog_info("=== END VMM DEBUG ===");
}

/**
 * @brief Verifica integrità di uno spazio virtuale
 */
bool vmm_check_integrity(vmm_space_t *space) {
  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation(space, "integrity check")) {
    return false;
  }

  // Delega all'implementazione arch-specific
  bool integrity_ok = arch_vmm_check_integrity(space);

  if (!integrity_ok) {
    klog_error("VMM: Integrity check fallito per spazio %p", space);
  }

  return integrity_ok;
}

/*
 * ============================================================================
 * STATISTICS AND DEBUGGING
 * ============================================================================
 */

/**
 * @brief Verifica che il VMM sia pronto per l'uso
 */
bool vmm_is_initialized(void) {
  return vmm_state.initialized;
}

/**
 * @brief Ottiene statistiche high-level del VMM
 */
void vmm_get_stats(u64 *spaces_created, u64 *total_mappings, u64 *total_unmappings) {
  if (spaces_created)
    *spaces_created = vmm_state.total_spaces_created;
  if (total_mappings)
    *total_mappings = vmm_state.total_mappings;
  if (total_unmappings)
    *total_unmappings = vmm_state.total_unmappings;
}

/**
 * @brief Stampa statistiche generiche del VMM
 */
void vmm_print_info(void) {
  if (!vmm_state.initialized) {
    klog_error("VMM: Richiesta statistiche su VMM non inizializzato");
    return;
  }

  klog_info("=== VMM INFORMATION ===");
  klog_info("Inizializzato: %s", vmm_state.initialized ? "Si" : "No");
  klog_info("Kernel space: %p", vmm_state.kernel_space);
  klog_info("Spazi creati: %lu", vmm_state.total_spaces_created);
  klog_info("Mappings totali: %lu", vmm_state.total_mappings);
  klog_info("Unmappings totali: %lu", vmm_state.total_unmappings);
  klog_info("=======================");
}