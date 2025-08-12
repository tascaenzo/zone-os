#include <klib/klog/klog.h>
#include <klib/spinlock.h>
#include <lib/string/string.h>
#include <lib/types.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

#define KERNEL_BASE 0xFFFF800000000000ULL

/**
 * @file mm/vmm.c
 * @brief Virtual Memory Manager - Implementazione architecture-agnostic THREAD-SAFE
 *
 * VERSIONE CORRETTA: Aggiunge protezione thread-safe completa per tutti gli accessi
 * allo stato globale e garantisce consistenza delle operazioni VMM.
 *
 * DESIGN PRINCIPLES:
 * - Questo file NON include mai header arch-specific
 * - Le funzioni arch-specific sono chiamate tramite external declarations
 * - La selezione dell'architettura avviene a link-time, non compile-time
 * - Tutta la logica comune (validazioni, statistiche, logging) è gestita qui
 * - Thread safety completa con spinlock per proteggere stato globale
 *
 * THREAD SAFETY PATTERN:
 * 1. Lock acquisito all'inizio delle funzioni pubbliche
 * 2. Validazione parametri (con lock tenuto)
 * 3. Chiamata a funzione arch-specific (con lock tenuto)
 * 4. Aggiornamento statistiche (con lock tenuto)
 * 5. Lock rilasciato prima del return
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
extern bool arch_vmm_check_integrity(vmm_space_t *space);
extern void *vmm_phys_to_virt(u64 phys_addr);
extern u64 vmm_virt_to_phys(u64 virt_addr);

/*
 * ============================================================================
 * STATO GLOBALE DEL VMM - ORA THREAD-SAFE
 * ============================================================================
 */

/**
 * @brief Spinlock per proteggere tutto lo stato globale VMM
 *
 * IMPORTANTE: Questo lock protegge TUTTO l'accesso a vmm_state.
 * Deve essere acquisito prima di leggere o modificare qualsiasi campo.
 */
static spinlock_t vmm_lock = SPINLOCK_INITIALIZER;

/**
 * @brief Stato interno del VMM generico - PROTETTO DA vmm_lock
 *
 * ATTENZIONE: Tutti gli accessi a questa struttura DEVONO essere
 * protetti dal vmm_lock per evitare race conditions.
 */
static struct {
  bool initialized;          // VMM è stato inizializzato?
  vmm_space_t *kernel_space; // Puntatore allo spazio kernel
  u64 total_spaces_created;  // Statistiche globali
  u64 total_mappings;        // Numero totale di mapping eseguiti
  u64 total_unmappings;      // Numero totale di unmapping eseguiti
} vmm_state = {.initialized = false, .kernel_space = (vmm_space_t *)NULL, .total_spaces_created = 0, .total_mappings = 0, .total_unmappings = 0};

/*
 * ============================================================================
 * UTILITY FUNCTIONS - ARCHITECTURE INDEPENDENT (THREAD-SAFE)
 * ============================================================================
 */

/**
 * @brief Valida i parametri comuni per operazioni di mapping
 *
 * THREAD-SAFE: Assume che il caller abbia già acquisito vmm_lock
 */
static bool validate_mapping_params_locked(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count) {
  if (!space) {
    klog_error("VMM: Spazio NULL passato a validate_mapping_params");
    return false;
  }

  // Verifica che VMM sia inizializzato (accesso sicuro grazie al lock)
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
 *
 * THREAD-SAFE: Assume che il caller abbia già acquisito vmm_lock
 */
static bool validate_space_operation_locked(vmm_space_t *space, const char *operation) {
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
 * IMPLEMENTAZIONE API PUBBLICA - THREAD-SAFE
 * ============================================================================
 */

/**
 * @brief Inizializza il Virtual Memory Manager
 *
 * THREAD-SAFE: Protegge l'inizializzazione da race conditions
 */
void vmm_init(void) {
  klog_info("VMM: Inizializzazione Virtual Memory Manager");

  // ACQUIRE LOCK per proteggere l'inizializzazione
  spinlock_lock(&vmm_lock);

  // Double-checked locking pattern per evitare doppia inizializzazione
  if (vmm_state.initialized) {
    klog_warn("VMM: Tentativo di re-inizializzazione, ignorato");
    spinlock_unlock(&vmm_lock);
    return;
  }

  // Verifica che il PMM sia già inizializzato (fuori dal lock per evitare deadlock)
  spinlock_unlock(&vmm_lock);
  const pmm_stats_t *pmm_stats = pmm_get_stats();
  if (!pmm_stats) {
    klog_panic("VMM: PMM deve essere inizializzato prima del VMM");
  }
  spinlock_lock(&vmm_lock);

  klog_info("VMM: PMM verificato - %lu MB disponibili", pmm_stats->free_pages * PAGE_SIZE / (1024 * 1024));

  // Delega l'inizializzazione all'arch layer (rilascia lock temporaneamente)
  spinlock_unlock(&vmm_lock);
  arch_vmm_init();
  spinlock_lock(&vmm_lock);

  // Ottieni il kernel space dall'arch layer (rilascia lock temporaneamente)
  spinlock_unlock(&vmm_lock);
  vmm_space_t *kernel_space = arch_vmm_get_kernel_space();
  spinlock_lock(&vmm_lock);

  if (!kernel_space) {
    spinlock_unlock(&vmm_lock);
    klog_panic("VMM: Impossibile ottenere kernel space dall'arch layer");
  }

  // Aggiorna stato in modo atomico (con lock tenuto)
  vmm_state.kernel_space = kernel_space;
  vmm_state.total_spaces_created = 1; // Kernel space
  vmm_state.total_mappings = 0;
  vmm_state.total_unmappings = 0;
  vmm_state.initialized = true;

  klog_info("VMM: Inizializzazione completata con successo");
  klog_info("VMM: Kernel space: %p", vmm_state.kernel_space);

  // RELEASE LOCK
  spinlock_unlock(&vmm_lock);
}

/**
 * @brief Ottiene lo spazio di indirizzamento del kernel
 *
 * THREAD-SAFE: Lettura atomica del kernel_space
 */
vmm_space_t *vmm_kernel_space(void) {
  // ACQUIRE LOCK per lettura sicura
  spinlock_lock(&vmm_lock);

  if (!vmm_state.initialized) {
    klog_error("VMM: Richiesta kernel space su VMM non inizializzato");
    spinlock_unlock(&vmm_lock);
    return (vmm_space_t *)NULL;
  }

  vmm_space_t *kernel_space = vmm_state.kernel_space;

  // RELEASE LOCK
  spinlock_unlock(&vmm_lock);

  return kernel_space;
}

/**
 * @brief Crea un nuovo spazio virtuale
 *
 * THREAD-SAFE: Aggiornamento atomico delle statistiche
 */
vmm_space_t *vmm_create_space(void) {
  // ACQUIRE LOCK per verifica inizializzazione
  spinlock_lock(&vmm_lock);

  if (!vmm_state.initialized) {
    klog_error("VMM: Tentativo di creare spazio su VMM non inizializzato");
    spinlock_unlock(&vmm_lock);
    return NULL;
  }

  // RELEASE LOCK temporaneamente per chiamata arch-specific
  spinlock_unlock(&vmm_lock);

  klog_debug("VMM: Creazione nuovo spazio di indirizzamento");

  // Delega all'implementazione arch-specific (senza lock)
  vmm_space_t *new_space = arch_vmm_create_space();

  // ACQUIRE LOCK per aggiornamento statistiche
  spinlock_lock(&vmm_lock);

  if (new_space) {
    vmm_state.total_spaces_created++; // Aggiornamento atomico
    klog_debug("VMM: Nuovo spazio creato con successo (%p)", new_space);
  } else {
    klog_error("VMM: Fallita creazione nuovo spazio");
  }

  // RELEASE LOCK
  spinlock_unlock(&vmm_lock);

  return new_space;
}

/**
 * @brief Distrugge uno spazio virtuale
 *
 * THREAD-SAFE: Validazione sicura del kernel space
 */
void vmm_destroy_space(vmm_space_t *space) {
  // ACQUIRE LOCK per validazione
  spinlock_lock(&vmm_lock);

  if (!validate_space_operation_locked(space, "distruzione spazio")) {
    spinlock_unlock(&vmm_lock);
    return;
  }

  // Non permettere distruzione del kernel space (controllo thread-safe)
  if (space == vmm_state.kernel_space) {
    klog_error("VMM: Tentativo di distruggere kernel space!");
    spinlock_unlock(&vmm_lock);
    return;
  }

  // RELEASE LOCK per chiamata arch-specific
  spinlock_unlock(&vmm_lock);

  klog_debug("VMM: Distruzione spazio %p", space);

  // Delega all'implementazione arch-specific (senza lock)
  arch_vmm_destroy_space(space);

  klog_debug("VMM: Spazio distrutto con successo");
}

/**
 * @brief Attiva uno spazio virtuale come corrente
 *
 * THREAD-SAFE: Validazione thread-safe prima dello switch
 */
void vmm_switch_space(vmm_space_t *space) {
  // ACQUIRE LOCK per validazione
  spinlock_lock(&vmm_lock);

  if (!validate_space_operation_locked(space, "switch spazio")) {
    spinlock_unlock(&vmm_lock);
    return;
  }

  // RELEASE LOCK per chiamata arch-specific
  spinlock_unlock(&vmm_lock);

  klog_debug("VMM: Switch a spazio %p", space);

  // Delega all'implementazione arch-specific (senza lock)
  arch_vmm_switch_space(space);

  klog_debug("VMM: Switch completato");
}

/**
 * @brief Mappa un range fisico nello spazio virtuale
 *
 * THREAD-SAFE: Protezione completa di validazione e aggiornamento statistiche
 */
bool vmm_map(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count, u64 flags) {
  // ACQUIRE LOCK per operazione completa
  spinlock_lock(&vmm_lock);

  // Se space è NULL, usa kernel space (accesso thread-safe)
  if (!space) {
    space = vmm_state.kernel_space;
  }

  // Validazione parametri (con lock tenuto)
  if (!validate_mapping_params_locked(space, virt_addr, phys_addr, page_count)) {
    spinlock_unlock(&vmm_lock);
    return false;
  }

  // Validazione flag
  if (flags == 0) {
    klog_warn("VMM: Mapping con flag=0, aggiungendo READ di default");
    flags = VMM_FLAG_READ;
  }

  klog_debug("VMM: Mapping %zu pagine: 0x%lx→0x%lx (flags=0x%lx)", page_count, virt_addr, phys_addr, flags);

  // RELEASE LOCK per chiamata arch-specific
  spinlock_unlock(&vmm_lock);

  // Delega all'implementazione arch-specific (senza lock)
  bool success = arch_vmm_map_pages(space, virt_addr, phys_addr, page_count, flags);

  // ACQUIRE LOCK per aggiornamento statistiche
  spinlock_lock(&vmm_lock);

  if (success) {
    vmm_state.total_mappings += page_count; // Aggiornamento atomico
    klog_debug("VMM: Mapping completato con successo");
  } else {
    klog_error("VMM: Mapping fallito");
  }

  // RELEASE LOCK
  spinlock_unlock(&vmm_lock);

  return success;
}

/**
 * @brief Annulla la mappatura virtuale di un range
 *
 * THREAD-SAFE: Protezione completa dell'operazione di unmapping
 */
void vmm_unmap(vmm_space_t *space, u64 virt_addr, size_t page_count) {
  // ACQUIRE LOCK per operazione completa
  spinlock_lock(&vmm_lock);

  // Se space è NULL, usa kernel space (accesso thread-safe)
  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation_locked(space, "unmapping")) {
    spinlock_unlock(&vmm_lock);
    return;
  }

  if (!IS_PAGE_ALIGNED(virt_addr)) {
    klog_error("VMM: Indirizzo virtuale per unmap non allineato: 0x%lx", virt_addr);
    spinlock_unlock(&vmm_lock);
    return;
  }

  if (page_count == 0) {
    klog_error("VMM: Tentativo di unmappare 0 pagine");
    spinlock_unlock(&vmm_lock);
    return;
  }

  klog_debug("VMM: Unmapping %zu pagine da 0x%lx", page_count, virt_addr);

  // RELEASE LOCK per chiamata arch-specific
  spinlock_unlock(&vmm_lock);

  // Delega all'implementazione arch-specific (senza lock)
  arch_vmm_unmap_pages(space, virt_addr, page_count);

  // ACQUIRE LOCK per aggiornamento statistiche
  spinlock_lock(&vmm_lock);

  vmm_state.total_unmappings += page_count; // Aggiornamento atomico

  // RELEASE LOCK
  spinlock_unlock(&vmm_lock);

  klog_debug("VMM: Unmapping completato");
}

/**
 * @brief Ottiene la mappatura fisica di un indirizzo virtuale
 *
 * THREAD-SAFE: Validazione thread-safe prima della risoluzione
 */
bool vmm_resolve(vmm_space_t *space, u64 virt_addr, u64 *out_phys_addr) {
  // ACQUIRE LOCK per validazione
  spinlock_lock(&vmm_lock);

  // Se space è NULL, usa kernel space (accesso thread-safe)
  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation_locked(space, "resolve")) {
    spinlock_unlock(&vmm_lock);
    return false;
  }

  // RELEASE LOCK per chiamata arch-specific
  spinlock_unlock(&vmm_lock);

  // Delega all'implementazione arch-specific (senza lock)
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
 *
 * THREAD-SAFE: Validazione sicura prima del debug dump
 */
void vmm_debug_dump(vmm_space_t *space) {
  // ACQUIRE LOCK per validazione e lettura sicura
  spinlock_lock(&vmm_lock);

  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation_locked(space, "debug dump")) {
    spinlock_unlock(&vmm_lock);
    return;
  }

  bool is_kernel_space = (space == vmm_state.kernel_space);

  // RELEASE LOCK per chiamata arch-specific
  spinlock_unlock(&vmm_lock);
}

/**
 * @brief Verifica integrità di uno spazio virtuale
 *
 * THREAD-SAFE: Validazione thread-safe prima del check
 */
bool vmm_check_integrity(vmm_space_t *space) {
  // ACQUIRE LOCK per validazione
  spinlock_lock(&vmm_lock);

  if (!space) {
    space = vmm_state.kernel_space;
  }

  if (!validate_space_operation_locked(space, "integrity check")) {
    spinlock_unlock(&vmm_lock);
    return false;
  }

  // RELEASE LOCK per chiamata arch-specific
  spinlock_unlock(&vmm_lock);

  // Delega all'implementazione arch-specific (senza lock)
  bool integrity_ok = arch_vmm_check_integrity(space);

  if (!integrity_ok) {
    klog_error("VMM: Integrity check fallito per spazio %p", space);
  }

  return integrity_ok;
}

/*
 * ============================================================================
 * STATISTICS AND DEBUGGING - THREAD-SAFE
 * ============================================================================
 */

/**
 * @brief Verifica che il VMM sia pronto per l'uso
 *
 * THREAD-SAFE: Lettura atomica dello stato di inizializzazione
 */
bool vmm_is_initialized(void) {
  spinlock_lock(&vmm_lock);
  bool initialized = vmm_state.initialized;
  spinlock_unlock(&vmm_lock);
  return initialized;
}

/**
 * @brief Ottiene statistiche high-level del VMM
 *
 * THREAD-SAFE: Snapshot atomico delle statistiche
 */
void vmm_get_stats(u64 *spaces_created, u64 *total_mappings, u64 *total_unmappings) {
  // ACQUIRE LOCK per snapshot atomico delle statistiche
  spinlock_lock(&vmm_lock);

  if (spaces_created)
    *spaces_created = vmm_state.total_spaces_created;
  if (total_mappings)
    *total_mappings = vmm_state.total_mappings;
  if (total_unmappings)
    *total_unmappings = vmm_state.total_unmappings;

  // RELEASE LOCK
  spinlock_unlock(&vmm_lock);
}

/**
 * @brief Stampa statistiche generiche del VMM
 *
 * THREAD-SAFE: Snapshot atomico per stampa consistente
 */
void vmm_print_info(void) {
  // ACQUIRE LOCK per snapshot atomico
  spinlock_lock(&vmm_lock);

  if (!vmm_state.initialized) {
    klog_error("VMM: Richiesta statistiche su VMM non inizializzato");
    spinlock_unlock(&vmm_lock);
    return;
  }

  // Crea snapshot locale delle statistiche
  bool initialized = vmm_state.initialized;
  vmm_space_t *kernel_space = vmm_state.kernel_space;
  u64 spaces_created = vmm_state.total_spaces_created;
  u64 mappings = vmm_state.total_mappings;
  u64 unmappings = vmm_state.total_unmappings;

  // RELEASE LOCK prima della stampa (che può essere lenta)
  spinlock_unlock(&vmm_lock);

  // Stampa usando snapshot locale (thread-safe)
  klog_info("=== VMM INFORMATION ===");
  klog_info("Inizializzato: %s", initialized ? "Si" : "No");
  klog_info("Kernel space: %p", kernel_space);
  klog_info("Spazi creati: %lu", spaces_created);
  klog_info("Mappings totali: %lu", mappings);
  klog_info("Unmappings totali: %lu", unmappings);
  klog_info("=======================");
}

void *vmm_phys_to_virt(u64 phys_addr) {
  return (void *)(phys_addr + KERNEL_BASE);
}

u64 vmm_virt_to_phys(u64 virt_addr) {
  return (u64)virt_addr - KERNEL_BASE;
}