#include "memory.h"
#include "vmm_defs.h"
#include <arch/x86_64/cpu/cpu.h>
#include <klib/klog/klog.h>
#include <lib/string/string.h>
#include <lib/types.h>
#include <mm/pmm.h>

/**
 * @file arch/x86_64/vmm_arch.c
 * @brief Implementazione x86_64-specifica del Virtual Memory Manager
 *
 * Questo file implementa tutte le operazioni di basso livello per la gestione
 * delle page table x86_64. Include la creazione/distruzione di spazi di
 * indirizzamento, mapping/unmapping di pagine, e traduzione di indirizzi.
 *
 * RESPONSABILITÀ:
 * - Gestione delle strutture page table x86_64 (PML4, PDPT, PD, PT)
 * - Allocazione/deallocazione page table dal PMM
 * - Traduzione indirizzi virtuali → fisici tramite page walk
 * - Gestione del registro CR3 e invalidazione TLB
 * - Debug e introspection delle page table
 */

/*
 * ============================================================================
 * STRUTTURE DATI INTERNE
 * ============================================================================
 */

/**
 * @brief Implementazione concreta di vmm_space_t per x86_64
 *
 * Questa struttura è l'implementazione reale del tipo opaco vmm_space_t
 * definito nell'header generico. Contiene tutti i dettagli specifici x86_64.
 */
struct vmm_space {
  vmm_x86_64_space_t arch; // Dati specifici x86_64
  u64 space_id;            // ID univoco per debug
  bool is_active;          // True se attualmente in uso
};

// Spazio di indirizzamento del kernel (singleton)
static struct vmm_space kernel_space = {
    .arch =
        {
            .pml4 = (vmm_x86_64_page_table_t *)NULL,
            .phys_pml4 = 0,
            .is_kernel_space = true,
            .mapped_pages = 0,
        },
    .space_id = 0,
    .is_active = true,
};

/*
 * ============================================================================
 * VARIABILI GLOBALI
 * ============================================================================
 */

static bool vmm_x86_64_initialized = false;
static u64 next_space_id = 1;
// Puntatore allo spazio attualmente attivo
static struct vmm_space *active_space = &kernel_space;

// True quando il direct map (phys→virt) è stato creato
static bool direct_map_ready = false;

// Statistiche per debug
// Statistiche per debug
static struct {
  u64 spaces_created;
  u64 spaces_destroyed;
  u64 pages_mapped;
  u64 pages_unmapped;
  u64 tlb_flushes;
} vmm_x86_64_stats = {
    .spaces_created = 0,
    .spaces_destroyed = 0,
    .pages_mapped = 0,
    .pages_unmapped = 0,
    .tlb_flushes = 0,
};

/*
 * ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Abilita il bit NX (No-Execute) nel registro EFER
 *
 * Implementazione separata per evitare problemi di inline assembly negli header.
 * Controlla se il processore supporta NX e lo abilita se necessario.
 */
void vmm_x86_64_enable_nx(void) {
  // Verifica se la CPU supporta il bit NX (CPUID bit 20 in EDX @ 0x80000001)
  if (!cpu_supports_nx()) {
    klog_warn("x86_64_vmm: NX non supportato dalla CPU. Proseguo senza NXE.");
    return;
  }

  klog_info("x86_64_vmm: NX supportato dalla CPU, abilito bit NXE in EFER");

  // Legge l'MSR EFER (0xC000_0080), setta bit 11 (NXE)
  u64 efer = cpu_rdmsr(0xC0000080); // MSR_EFER

  if (!(efer & (1ULL << 11))) {
    efer |= (1ULL << 11); // Setta bit NXE
    cpu_wrmsr(0xC0000080, efer);
    klog_info("x86_64_vmm: Bit NXE abilitato con successo (EFER = 0x%016lx)", efer);
  } else {
    klog_info("x86_64_vmm: Bit NXE già abilitato (EFER = 0x%016lx)", efer);
  }
}

/**
 * @brief Alloca una page table vuota dal PMM
 *
 * Alloca una pagina fisica, la azzera, e restituisce sia l'indirizzo
 * virtuale che fisico. Essenziale per creare nuove page table.
 *
 * @param virt_addr[out] Indirizzo virtuale della page table
 * @param phys_addr[out] Indirizzo fisico della page table
 * @return true se allocazione riuscita
 */
static bool alloc_page_table(vmm_x86_64_page_table_t **virt_addr, u64 *phys_addr) {
  // Alloca pagina fisica tramite PMM
  void *page = NULL;
  if (!direct_map_ready) {
    // Durante l'early boot è garantita l'identity mapping solo sotto 4GB
    page = pmm_alloc_pages_in_range(1, 0, 0x100000000ULL);
  }
  if (!page)
    page = pmm_alloc_page();
  if (!page) {
    klog_error("x86_64_vmm: Impossibile allocare page table dal PMM");
    return false;
  }

  *phys_addr = (u64)page;

  // Validazione indirizzo fisico
  if (!arch_memory_region_valid(*phys_addr, PAGE_SIZE)) {
    klog_error("x86_64_vmm: Indirizzo page table non valido: 0x%lx", *phys_addr);
    pmm_free_page(page);
    return false;
  }

  if (direct_map_ready) {
    *virt_addr = (vmm_x86_64_page_table_t *)VMM_X86_64_PHYS_TO_VIRT(*phys_addr);
  } else {
    // Durante l'early boot il bootloader fornisce identity mapping
    *virt_addr = (vmm_x86_64_page_table_t *)page;
  }

  // Azzera la page table (tutte le entry non presenti)
  memset(*virt_addr, 0, PAGE_SIZE);

  return true;
}

/**
 * @brief Libera una page table e la restituisce al PMM
 *
 * @param virt_addr Indirizzo virtuale della page table da liberare
 */
static void free_page_table(vmm_x86_64_page_table_t *virt_addr) {
  if (virt_addr) {
    void *phys = direct_map_ready ? (void *)VMM_X86_64_VIRT_TO_PHYS(virt_addr) : (void *)virt_addr;
    pmm_free_page(phys);
  }
}

/**
 * @brief Libera ricorsivamente le page table a partire da un livello
 */
static void free_page_tables_recursive(vmm_x86_64_page_table_t *table, int level) {
  if (!table || level <= 0)
    return;

  if (level > 1) {
    for (int i = 0; i < VMM_X86_64_ENTRIES_PER_TABLE; i++) {
      vmm_x86_64_pte_t *entry = &table->entries[i];
      if (VMM_X86_64_PTE_PRESENT(entry->raw)) {
        vmm_x86_64_page_table_t *child = direct_map_ready ? (vmm_x86_64_page_table_t *)VMM_X86_64_PHYS_TO_VIRT(VMM_X86_64_PTE_ADDR(entry->raw))
                                                          : (vmm_x86_64_page_table_t *)(uptr)VMM_X86_64_PTE_ADDR(entry->raw);
        free_page_tables_recursive(child, level - 1);
      }
    }
  }

  free_page_table(table);
}

/**
 * @brief Esegue il page walk per trovare una PTE
 *
 * Naviga attraverso i 4 livelli delle page table x86_64 per trovare
 * la Page Table Entry corrispondente a un indirizzo virtuale.
 *
 * @param space Spazio di indirizzamento
 * @param virt_addr Indirizzo virtuale da cercare
 * @param create_missing Se true, crea page table mancanti
 * @return Puntatore alla PTE, o NULL se non trovata/creabile
 */
static vmm_x86_64_pte_t *page_walk(vmm_space_t *space, u64 virt_addr, bool create_missing) {
  if (!space || !space->arch.pml4) {
    return (vmm_x86_64_pte_t *)NULL;
  }

  bool new_pdpt = false;
  bool new_pd = false;

  // Estrai gli indici dai bit dell'indirizzo virtuale
  u64 pml4_idx = VMM_X86_64_PML4_INDEX(virt_addr);
  u64 pdpt_idx = VMM_X86_64_PDPT_INDEX(virt_addr);
  u64 pd_idx = VMM_X86_64_PD_INDEX(virt_addr);
  u64 pt_idx = VMM_X86_64_PT_INDEX(virt_addr);

  // LIVELLO 1: PML4 (Page Map Level 4)
  vmm_x86_64_pte_t *pml4_entry = &space->arch.pml4->entries[pml4_idx];
  vmm_x86_64_page_table_t *pdpt;

  if (!VMM_X86_64_PTE_PRESENT(pml4_entry->raw)) {
    if (!create_missing)
      return (vmm_x86_64_pte_t *)NULL;

    // Crea nuovo PDPT
    u64 pdpt_phys;
    if (!alloc_page_table(&pdpt, &pdpt_phys)) {
      return (vmm_x86_64_pte_t *)NULL;
    }

    new_pdpt = true;

    // Imposta la entry PML4 (kernel: writable, present)
    u64 flags = VMM_X86_64_PRESENT | VMM_X86_64_WRITABLE;
    if (!space->arch.is_kernel_space) {
      flags |= VMM_X86_64_USER;
    }
    pml4_entry->raw = VMM_X86_64_MAKE_PTE(pdpt_phys, flags);
  } else {
    u64 pdpt_phys = VMM_X86_64_PTE_ADDR(pml4_entry->raw);
    if (!arch_memory_region_valid(pdpt_phys, PAGE_SIZE)) {
      klog_error("x86_64_vmm: Invalid PDPT address: 0x%lx", pdpt_phys);
      return (vmm_x86_64_pte_t *)NULL;
    }
    pdpt = direct_map_ready ? (vmm_x86_64_page_table_t *)VMM_X86_64_PHYS_TO_VIRT(pdpt_phys) : (vmm_x86_64_page_table_t *)(uptr)pdpt_phys;
  }

  // LIVELLO 2: PDPT (Page Directory Pointer Table)
  vmm_x86_64_pte_t *pdpt_entry = &pdpt->entries[pdpt_idx];
  vmm_x86_64_page_table_t *pd;

  if (!VMM_X86_64_PTE_PRESENT(pdpt_entry->raw)) {
    if (!create_missing)
      return (vmm_x86_64_pte_t *)NULL;

    // Crea nuovo Page Directory
    u64 pd_phys;
    if (!alloc_page_table(&pd, &pd_phys)) {
      if (new_pdpt) {
        free_page_table(pdpt);
        pml4_entry->raw = 0;
      }
      return (vmm_x86_64_pte_t *)NULL;
    }

    new_pd = true;

    u64 flags = VMM_X86_64_PRESENT | VMM_X86_64_WRITABLE;
    if (!space->arch.is_kernel_space) {
      flags |= VMM_X86_64_USER;
    }
    pdpt_entry->raw = VMM_X86_64_MAKE_PTE(pd_phys, flags);
  } else {
    u64 pd_phys = VMM_X86_64_PTE_ADDR(pdpt_entry->raw);
    if (!arch_memory_region_valid(pd_phys, PAGE_SIZE)) {
      klog_error("x86_64_vmm: Invalid PD address: 0x%lx", pd_phys);
      return (vmm_x86_64_pte_t *)NULL;
    }
    pd = direct_map_ready ? (vmm_x86_64_page_table_t *)VMM_X86_64_PHYS_TO_VIRT(pd_phys) : (vmm_x86_64_page_table_t *)(uptr)pd_phys;
  }

  // LIVELLO 3: PD (Page Directory)
  vmm_x86_64_pte_t *pd_entry = &pd->entries[pd_idx];
  vmm_x86_64_page_table_t *pt;

  if (!VMM_X86_64_PTE_PRESENT(pd_entry->raw)) {
    if (!create_missing)
      return (vmm_x86_64_pte_t *)NULL;

    // Crea nuovo Page Table
    u64 pt_phys;
    if (!alloc_page_table(&pt, &pt_phys)) {
      if (new_pd) {
        free_page_table(pd);
        pdpt_entry->raw = 0;
      }
      if (new_pdpt) {
        free_page_table(pdpt);
        pml4_entry->raw = 0;
      }
      return (vmm_x86_64_pte_t *)NULL;
    }

    u64 flags = VMM_X86_64_PRESENT | VMM_X86_64_WRITABLE;
    if (!space->arch.is_kernel_space) {
      flags |= VMM_X86_64_USER;
    }
    pd_entry->raw = VMM_X86_64_MAKE_PTE(pt_phys, flags);
  } else {
    u64 pt_phys = VMM_X86_64_PTE_ADDR(pd_entry->raw);
    if (!arch_memory_region_valid(pt_phys, PAGE_SIZE)) {
      klog_error("x86_64_vmm: Invalid PT address: 0x%lx", pt_phys);
      return (vmm_x86_64_pte_t *)NULL;
    }
    pt = direct_map_ready ? (vmm_x86_64_page_table_t *)VMM_X86_64_PHYS_TO_VIRT(pt_phys) : (vmm_x86_64_page_table_t *)(uptr)pt_phys;
  }

  // LIVELLO 4: PT (Page Table) - ritorna la PTE finale
  return &pt->entries[pt_idx];
}

/*
 * ============================================================================
 * API ARCH-SPECIFIC IMPLEMENTATION
 * ============================================================================
 */

/**
 * @brief Inizializza il paging x86_64
 *
 * Setup iniziale del sistema di paging. Crea lo spazio kernel e abilita
 * le funzionalità necessarie come il bit NX.
 */
void vmm_x86_64_init_paging(void) {
  klog_info("x86_64_vmm: Inizializzazione paging x86_64");

  // Verifica che il PMM sia inizializzato
  if (!pmm_get_stats()) {
    klog_panic("x86_64_vmm: PMM deve essere inizializzato prima del VMM");
  }

  // Abilita il bit NX (No-Execute) se supportato
  vmm_x86_64_enable_nx();
  klog_info("x86_64_vmm: Bit NX abilitato");

  // Crea lo spazio di indirizzamento del kernel
  if (!alloc_page_table(&kernel_space.arch.pml4, &kernel_space.arch.phys_pml4)) {
    klog_panic("x86_64_vmm: Impossibile creare PML4 kernel");
  }

  kernel_space.arch.is_kernel_space = true;
  kernel_space.arch.mapped_pages = 0;
  kernel_space.space_id = 0; // ID speciale per kernel
  kernel_space.is_active = true;
  active_space = &kernel_space;

  // Copia le mappature attualmente attive (fornite dal bootloader)
  // così da preservare la high-half del kernel. Il CR3 corrente punta
  // alle page table di Limine che già mappano il kernel nelle zone
  // superiori dello spazio virtuale.
  u64 boot_cr3 = vmm_x86_64_read_cr3();
  vmm_x86_64_page_table_t *boot_pml4 = (vmm_x86_64_page_table_t *)(uptr)boot_cr3;
  memcpy(kernel_space.arch.pml4, boot_pml4, PAGE_SIZE);

  // Attiva immediatamente le nostre nuove page table
  vmm_x86_64_write_cr3(kernel_space.arch.phys_pml4);
  vmm_x86_64_flush_tlb();

  klog_info("x86_64_vmm: Spazio kernel creato (PML4 fisico: 0x%016lx)", kernel_space.arch.phys_pml4);

  // Segna inizializzato prima di mappare il direct map
  vmm_x86_64_initialized = true;
  vmm_x86_64_stats.spaces_created = 1; // Kernel space

  memory_region_t regions[ARCH_MAX_MEMORY_REGIONS];
  size_t region_count = arch_memory_detect_regions(regions, ARCH_MAX_MEMORY_REGIONS);
  u64 mapped_pages = 0;

  for (size_t i = 0; i < region_count; i++) {
    memory_region_t *r = &regions[i];
    if (r->type == MEMORY_USABLE || r->type == MEMORY_BOOTLOADER_RECLAIMABLE || r->type == MEMORY_ACPI_RECLAIMABLE) {
      u64 base = PAGE_ALIGN_DOWN(r->base);
      u64 end = PAGE_ALIGN_UP(r->base + r->length);
      u64 pages = (end - base) / PAGE_SIZE;
      if (!vmm_x86_64_map_pages(&kernel_space, VMM_X86_64_DIRECT_MAP + base, base, pages, VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_GLOBAL)) {
        klog_panic("x86_64_vmm: impossibile creare direct map per 0x%lx", base);
      }
      mapped_pages += pages;
    }
  }

  direct_map_ready = true;
  if (mapped_pages > 0) {
    klog_info("x86_64_vmm: Direct map abilitato (%lu MB)", (mapped_pages * PAGE_SIZE) / (1024 * 1024));
  }
}

/**
 * @brief Crea un nuovo spazio di indirizzamento x86_64
 *
 * Alloca una nuova PML4 e inizializza un nuovo spazio virtuale.
 * Tipicamente usato per creare spazi utente per i processi.
 */
vmm_space_t *vmm_x86_64_create_space(void) {
  if (!vmm_x86_64_initialized) {
    klog_error("x86_64_vmm: VMM non inizializzato");
    return (vmm_space_t *)NULL;
  }

  // Alloca struttura per il nuovo spazio
  vmm_space_t *space = (vmm_space_t *)pmm_alloc_page();
  if (!space) {
    klog_error("x86_64_vmm: Impossibile allocare vmm_space");
    return (vmm_space_t *)NULL;
  }

  // Azzera la struttura
  memset(space, 0, sizeof(struct vmm_space));

  // Alloca PML4 per il nuovo spazio
  if (!alloc_page_table(&space->arch.pml4, &space->arch.phys_pml4)) {
    klog_error("x86_64_vmm: Impossibile allocare PML4");
    pmm_free_page(space);
    return NULL;
  }

  // Inizializza metadati
  space->arch.is_kernel_space = false;
  space->arch.mapped_pages = 0;
  space->space_id = next_space_id++;
  space->is_active = false;

  // Copia le mappature del kernel (upper half) dalla PML4 del kernel
  size_t kernel_idx = VMM_X86_64_PML4_INDEX(VMM_X86_64_KERNEL_BASE);
  for (size_t i = kernel_idx; i < VMM_X86_64_ENTRIES_PER_TABLE; i++) {
    space->arch.pml4->entries[i] = kernel_space.arch.pml4->entries[i];
  }

  vmm_x86_64_stats.spaces_created++;

  klog_debug("x86_64_vmm: Creato spazio ID=%lu (PML4=0x%016lx)", space->space_id, space->arch.phys_pml4);

  return space;
}

/**
 * @brief Distrugge uno spazio di indirizzamento x86_64
 *
 * Libera ricorsivamente tutte le page table e la struttura dello spazio.
 * ATTENZIONE: Non libera le pagine fisiche mappate!
 */
void vmm_x86_64_destroy_space(vmm_space_t *space) {
  if (!space || space == &kernel_space) {
    klog_warn("x86_64_vmm: Tentativo di distruggere kernel space o NULL");
    return;
  }

  if (space->is_active) {
    klog_warn("x86_64_vmm: Distruggendo spazio attivo ID=%lu", space->space_id);
    vmm_x86_64_switch_space(&kernel_space); // Force switch + TLB flush
  }

  klog_debug("x86_64_vmm: Distruggendo spazio ID=%lu", space->space_id);

  if (space->arch.pml4) {
    free_page_tables_recursive(space->arch.pml4, 4);
  }

  // Libera la struttura dello spazio
  pmm_free_page(space);

  vmm_x86_64_stats.spaces_destroyed++;
}

/**
 * @brief Attiva uno spazio di indirizzamento (carica CR3)
 *
 * Carica l'indirizzo fisico della PML4 nel registro CR3, causando
 * il switch immediato dello spazio di indirizzamento.
 */
void vmm_x86_64_switch_space(vmm_space_t *space) {
  if (!space || !space->arch.pml4) {
    klog_error("x86_64_vmm: Tentativo di switch a spazio non valido");
    return;
  }

  // Leggi CR3 corrente per confronto
  u64 current_cr3 = vmm_x86_64_read_cr3();
  u64 new_cr3 = space->arch.phys_pml4;

  // Evita switch inutili
  if (current_cr3 == new_cr3) {
    return;
  }

  // Aggiorna tracking dello spazio attivo
  if (active_space) {
    active_space->is_active = false;
  }
  space->is_active = true;
  active_space = (struct vmm_space *)space;
  vmm_x86_64_write_cr3(new_cr3);

  vmm_x86_64_stats.tlb_flushes++;

  klog_debug("x86_64_vmm: Switch a spazio ID=%lu (CR3=0x%016lx)", space->space_id, new_cr3);
}

/**
 * @brief Mappa un range di pagine virtuali → fisiche
 *
 * Implementazione arch-specific del mapping. Crea le page table necessarie
 * e imposta le PTE con i flag appropriati.
 */
bool vmm_x86_64_map_pages(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count, u64 flags) {
  if (!space || !vmm_x86_64_initialized) {
    // klog_error("x86_64_vmm: Parametri non validi per map_pages");
    return false;
  }

  if (!IS_PAGE_ALIGNED(virt_addr) || !IS_PAGE_ALIGNED(phys_addr)) {
    // klog_error("x86_64_vmm: Indirizzi non allineati: virt=0x%lx, phys=0x%lx", virt_addr, phys_addr);
    return false;
  }

  if (!VMM_X86_64_IS_CANONICAL(virt_addr)) {
    // klog_error("x86_64_vmm: Indirizzo virtuale non canonico: 0x%lx", virt_addr);
    return false;
  }

  u64 x86_flags = vmm_x86_64_convert_flags(flags);

  // klog_debug("x86_64_vmm: Mapping %zu pagine: 0x%lx→0x%lx (flags=0x%lx)", page_count, virt_addr, phys_addr, x86_flags);

  bool need_tlb_flush = false;

  for (size_t i = 0; i < page_count; i++) {
    u64 curr_virt = virt_addr + (i * PAGE_SIZE);
    u64 curr_phys = phys_addr + (i * PAGE_SIZE);

    vmm_x86_64_pte_t *pte = page_walk(space, curr_virt, true);
    if (!pte) {
      // klog_error("x86_64_vmm: Page walk fallito per 0x%lx", curr_virt);
      for (size_t j = 0; j < i; j++) {
        u64 rb_virt = virt_addr + (j * PAGE_SIZE);
        vmm_x86_64_pte_t *rb_pte = page_walk(space, rb_virt, false);
        if (rb_pte && VMM_X86_64_PTE_PRESENT(rb_pte->raw)) {
          rb_pte->raw = 0;
          if (space->is_active) {
            vmm_x86_64_invlpg(rb_virt);
          }
          space->arch.mapped_pages--;
          vmm_x86_64_stats.pages_unmapped++;
          if (vmm_x86_64_stats.pages_mapped > 0)
            vmm_x86_64_stats.pages_mapped--;
        }
      }
      return false;
    }

    if (VMM_X86_64_PTE_PRESENT(pte->raw)) {
      klog_warn("x86_64_vmm: Pagina 0x%lx già mappata (sovrascrittura)", curr_virt);
    }

    pte->raw = VMM_X86_64_MAKE_PTE(curr_phys, x86_flags);

    need_tlb_flush = need_tlb_flush || space->is_active;

    space->arch.mapped_pages++;
    vmm_x86_64_stats.pages_mapped++;
  }

  if (need_tlb_flush && space->is_active) {
    vmm_x86_64_flush_tlb();
  }

  // klog_debug("x86_64_vmm: Mapping completato con successo");
  return true;
}

/**
 * @brief Rimuove mapping di un range di pagine virtuali
 *
 * Azzera le PTE corrispondenti e invalida il TLB. NON libera le page table
 * intermedie per semplicità (potrebbero essere condivise).
 */
void vmm_x86_64_unmap_pages(vmm_space_t *space, u64 virt_addr, size_t page_count) {
  if (!space || !vmm_x86_64_initialized) {
    klog_error("x86_64_vmm: Parametri non validi per unmap_pages");
    return;
  }

  if (!IS_PAGE_ALIGNED(virt_addr)) {
    klog_error("x86_64_vmm: Indirizzo virtuale non allineato: 0x%lx", virt_addr);
    return;
  }

  klog_debug("x86_64_vmm: Unmapping %zu pagine da 0x%lx", page_count, virt_addr);

  for (size_t i = 0; i < page_count; i++) {
    u64 curr_virt = virt_addr + (i * PAGE_SIZE);

    // Trova la PTE (senza creare page table mancanti)
    vmm_x86_64_pte_t *pte = page_walk(space, curr_virt, false);
    if (!pte || !VMM_X86_64_PTE_PRESENT(pte->raw)) {
      klog_debug("x86_64_vmm: Pagina 0x%lx non mappata, saltando", curr_virt);
      continue;
    }

    // Azzera la PTE (rimuove mapping)
    pte->raw = 0;

    // Invalida la pagina nel TLB se lo spazio è attivo
    if (space->is_active) {
      vmm_x86_64_invlpg(curr_virt);
    }

    space->arch.mapped_pages--;
    vmm_x86_64_stats.pages_unmapped++;
  }

  klog_debug("x86_64_vmm: Unmapping completato");
}

/**
 * @brief Risolve un indirizzo virtuale in fisico
 *
 * Esegue il page walk e restituisce l'indirizzo fisico corrispondente
 * se la pagina è mappata.
 */
bool vmm_x86_64_resolve(vmm_space_t *space, u64 virt_addr, u64 *phys_addr) {
  if (!space || !vmm_x86_64_initialized) {
    return false;
  }

  // Trova la PTE senza creare page table mancanti
  vmm_x86_64_pte_t *pte = page_walk(space, virt_addr, false);
  if (!pte || !VMM_X86_64_PTE_PRESENT(pte->raw)) {
    return false; // Pagina non mappata
  }

  // Estrae l'indirizzo fisico base dalla PTE
  u64 phys_base = VMM_X86_64_PTE_ADDR(pte->raw);

  // Aggiunge l'offset nella pagina
  u64 page_offset = VMM_X86_64_PAGE_OFFSET(virt_addr);

  if (phys_addr) {
    *phys_addr = phys_base + page_offset;
  }

  return true;
}

/**
 * @brief Debug dump delle page table
 *
 * Stampa informazioni dettagliate sulle page table di uno spazio.
 * Utile per debugging e analisi della memoria virtuale.
 */
static const char *level_names[] = VMM_X86_64_LEVEL_NAMES;
static const u64 level_sizes[] = {VMM_X86_64_PT_SIZE, VMM_X86_64_PD_SIZE, VMM_X86_64_PDPT_SIZE, VMM_X86_64_PML4_SIZE};

static void dump_table_recursive(vmm_x86_64_page_table_t *table, int level, u64 virt_base) {
  if (!table || level <= 0)
    return;

  for (int i = 0; i < VMM_X86_64_ENTRIES_PER_TABLE; i++) {
    vmm_x86_64_pte_t *entry = &table->entries[i];
    if (!VMM_X86_64_PTE_PRESENT(entry->raw))
      continue;

    u64 child_phys = VMM_X86_64_PTE_ADDR(entry->raw);
    u64 virt_addr = virt_base + ((u64)i * level_sizes[level - 1]);

    klog_info("[%s %3d] VA 0x%016lx -> PA 0x%016lx flags=0x%016lx", level_names[level - 1], i, virt_addr, child_phys, entry->raw);

    if (level > 1 && !entry->page_size) {
      vmm_x86_64_page_table_t *child = direct_map_ready ? (vmm_x86_64_page_table_t *)VMM_X86_64_PHYS_TO_VIRT(child_phys) : (vmm_x86_64_page_table_t *)(uptr)child_phys;
      dump_table_recursive(child, level - 1, virt_addr);
    }
  }
}

static bool integrity_walk(vmm_x86_64_page_table_t *table, int level, bool kernel_space, u64 *count) {
  if (!table || level <= 0)
    return true;

  for (int i = 0; i < VMM_X86_64_ENTRIES_PER_TABLE; i++) {
    vmm_x86_64_pte_t *entry = &table->entries[i];
    if (!VMM_X86_64_PTE_PRESENT(entry->raw))
      continue;

    if (!IS_PAGE_ALIGNED(VMM_X86_64_PTE_ADDR(entry->raw))) {
      klog_error("x86_64_vmm: entry L%d[%d] non allineata", level, i);
      return false;
    }

    if (kernel_space && (entry->raw & VMM_X86_64_USER)) {
      klog_error("x86_64_vmm: flag USER su entry kernel L%d[%d]", level, i);
      return false;
    }

    if (level > 1 && !entry->page_size) {
      vmm_x86_64_page_table_t *child =
          direct_map_ready ? (vmm_x86_64_page_table_t *)VMM_X86_64_PHYS_TO_VIRT(VMM_X86_64_PTE_ADDR(entry->raw)) : (vmm_x86_64_page_table_t *)(uptr)VMM_X86_64_PTE_ADDR(entry->raw);
      if (!integrity_walk(child, level - 1, kernel_space, count))
        return false;
    } else if (level == 1) {
      (*count)++;
    }
  }

  return true;
}

void vmm_x86_64_debug_dump(vmm_space_t *space) {
  if (!space) {
    klog_info("=== VMM DEBUG: NULL SPACE ===");
    return;
  }

  klog_info("=== VMM DEBUG: SPAZIO ID=%lu ===", space->space_id);
  klog_info("PML4 fisico: 0x%016lx", space->arch.phys_pml4);
  klog_info("PML4 virtuale: %p", space->arch.pml4);
  klog_info("Pagine mappate: %lu", space->arch.mapped_pages);
  klog_info("Kernel space: %s", space->arch.is_kernel_space ? "Si" : "No");
  klog_info("Attivo: %s", space->is_active ? "Si" : "No");

  dump_table_recursive(space->arch.pml4, 4, 0);

  klog_info("=== END VMM DEBUG ===");
}

/*
 * ============================================================================
 * FUNZIONI DI SUPPORTO PER L'API GENERICA
 * ============================================================================
 */

/**
 * @brief Ritorna lo spazio kernel (per vmm_kernel_space())
 */
vmm_space_t *vmm_x86_64_get_kernel_space(void) {
  return (vmm_space_t *)&kernel_space;
}

/**
 * @brief Stampa statistiche arch-specific
 */
void vmm_x86_64_print_stats(void) {
  klog_info("=== VMM x86_64 STATISTICS ===");
  klog_info("Spazi creati: %lu", vmm_x86_64_stats.spaces_created);
  klog_info("Spazi distrutti: %lu", vmm_x86_64_stats.spaces_destroyed);
  klog_info("Pagine mappate: %lu", vmm_x86_64_stats.pages_mapped);
  klog_info("Pagine unmappate: %lu", vmm_x86_64_stats.pages_unmapped);
  klog_info("TLB flush: %lu", vmm_x86_64_stats.tlb_flushes);
  klog_info("=============================");
}

/**
 * @brief Verifica integrità di uno spazio x86_64
 *
 * Controlla che le strutture interne siano consistenti.
 */
bool vmm_x86_64_check_integrity(vmm_space_t *space) {
  if (!space) {
    return false;
  }

  // Verifica che la PML4 sia allocata
  if (!space->arch.pml4 || space->arch.phys_pml4 == 0) {
    klog_error("x86_64_vmm: PML4 non valida nello spazio ID=%lu", space->space_id);
    return false;
  }

  // Verifica allineamento PML4 sia virtuale che fisico
  if (!IS_PAGE_ALIGNED(space->arch.phys_pml4) || !IS_PAGE_ALIGNED((u64)space->arch.pml4)) {
    klog_error("x86_64_vmm: PML4 non allineata: virt=%p phys=0x%lx", space->arch.pml4, space->arch.phys_pml4);
    return false;
  }

  u64 counted_pages = 0;
  bool ok = integrity_walk(space->arch.pml4, 4, space->arch.is_kernel_space, &counted_pages);

  if (ok && counted_pages != space->arch.mapped_pages) {
    klog_error("x86_64_vmm: mismatch pagine mappate: %lu (contate) vs %lu (cache)", counted_pages, space->arch.mapped_pages);
    ok = false;
  }

  return ok;
}

// BINDING PER INTERFACCIA GENERICA VMM
void arch_vmm_init(void) {
  vmm_x86_64_init_paging();
}
vmm_space_t *arch_vmm_get_kernel_space(void) {
  return vmm_x86_64_get_kernel_space();
}
vmm_space_t *arch_vmm_create_space(void) {
  return vmm_x86_64_create_space();
}
void arch_vmm_destroy_space(vmm_space_t *space) {
  vmm_x86_64_destroy_space(space);
}
void arch_vmm_switch_space(vmm_space_t *space) {
  vmm_x86_64_switch_space(space);
}
bool arch_vmm_map_pages(vmm_space_t *s, u64 v, u64 p, size_t n, u64 f) {
  return vmm_x86_64_map_pages(s, v, p, n, f);
}
void arch_vmm_unmap_pages(vmm_space_t *s, u64 v, size_t n) {
  vmm_x86_64_unmap_pages(s, v, n);
}
bool arch_vmm_resolve(vmm_space_t *s, u64 v, u64 *out) {
  return vmm_x86_64_resolve(s, v, out);
}
void arch_vmm_debug_dump(vmm_space_t *space) {
  vmm_x86_64_debug_dump(space);
}
bool arch_vmm_check_integrity(vmm_space_t *space) {
  return vmm_x86_64_check_integrity(space);
}
