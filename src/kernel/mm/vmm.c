/**
 * @file mm/vmm.c
 * @brief Virtual Memory Manager — core arch‑agnostico con backend <arch/vmm.h>
 *
 * Fornisce API di alto livello per spazi di indirizzamento e (un)mapping.
 * La logica HW‑specific (page tables, CR3, invlpg, HHDM…) è delegata al
 * backend architetturale dichiarato in <arch/vmm.h>.
 *
 * Thread‑safe: stato globale protetto da spinlock.
 *
 * @author Enzo Tasca
 * @date 2025
 * @license MIT
 */

#include "page.h"        /* mm_is_page_aligned(), PAGE_ALIGN_* */
#include <arch/memory.h> /* arch_memory_page_size() */
#include <arch/vmm.h>    /* arch_vmm_* backend e tipi arch_vmm_* */
#include <klib/klog/klog.h>
#include <klib/spinlock/spinlock.h>
#include <lib/stdbool.h>
#include <lib/stddef.h>
#include <lib/string/string.h>
#include <lib/types.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

/* -------------------------------------------------------------------------- */
/*                                STATO GLOBALE                                */
/* -------------------------------------------------------------------------- */

static spinlock_t vmm_lock = SPINLOCK_INITIALIZER;

static struct {
  bool initialized;
  vmm_space_t *kernel_space;
  u64 total_spaces_created;
  u64 total_mappings;
  u64 total_unmappings;
} vmm_state = {
    .initialized = false,
    .kernel_space = (vmm_space_t *)0,
    .total_spaces_created = 0,
    .total_mappings = 0,
    .total_unmappings = 0,
};

/* -------------------------------------------------------------------------- */
/*                             ADAPTER FLAG VMM→ARCH                           */
/* -------------------------------------------------------------------------- */

static inline arch_vmm_flags_t vmm_to_arch_flags(u64 vflags) {
  arch_vmm_flags_t a = 0;
  /* READ è implicito con PTE present; NX se manca EXEC */
  if (vflags & VMM_FLAG_WRITE)
    a |= ARCH_VMM_WRITE;
  if (vflags & VMM_FLAG_USER)
    a |= ARCH_VMM_USER;
  if (!(vflags & VMM_FLAG_EXEC))
    a |= ARCH_VMM_NOEXEC;
  if (vflags & VMM_FLAG_GLOBAL)
    a |= ARCH_VMM_GLOBAL;
  if (vflags & VMM_FLAG_NO_CACHE)
    a |= ARCH_VMM_UC; /* best‑effort */
  return a;
}

/* -------------------------------------------------------------------------- */
/*                              VALIDAZIONE COMUNE                             */
/* -------------------------------------------------------------------------- */

static bool validate_mapping_params_locked(vmm_space_t *space, u64 va, u64 pa, size_t pages) {
  if (!vmm_state.initialized) {
    klog_error("VMM: non inizializzato");
    return false;
  }
  if (!space) {
    klog_error("VMM: spazio NULL");
    return false;
  }
  if (!mm_is_page_aligned(va)) {
    klog_error("VMM: VA non allineato 0x%lx", va);
    return false;
  }
  if (!mm_is_page_aligned(pa)) {
    klog_error("VMM: PA non allineato 0x%lx", pa);
    return false;
  }
  if (pages == 0) {
    klog_error("VMM: pages=0 non valido");
    return false;
  }
  if (pages > VMM_MAX_MAPPING_PAGES) {
    u64 mb = (pages * arch_memory_page_size()) / (1024UL * 1024UL);
    klog_warn("VMM: mapping molto grande: %zu pagine (~%lu MB)", pages, mb);
  }
  return true;
}

static bool validate_space_locked(vmm_space_t *space, const char *op) {
  if (!vmm_state.initialized) {
    klog_error("VMM: %s con VMM non inizializzato", op);
    return false;
  }
  if (!space) {
    klog_error("VMM: %s su spazio NULL", op);
    return false;
  }
  return true;
}

/* -------------------------------------------------------------------------- */
/*                                 API PUBBLICA                                */
/* -------------------------------------------------------------------------- */

void vmm_init(void) {
  klog_info("VMM: init");
  spinlock_lock(&vmm_lock);
  if (vmm_state.initialized) {
    spinlock_unlock(&vmm_lock);
    klog_warn("VMM: già inizializzato");
    return;
  }
  spinlock_unlock(&vmm_lock);

  /* Richiede PMM già pronto */
  const pmm_stats_t *ps = pmm_get_stats();
  if (!ps)
    klog_panic("VMM: PMM non inizializzato");

  arch_vmm_init();

  vmm_space_t *kspace = arch_vmm_get_kernel_space();
  if (!kspace)
    klog_panic("VMM: backend non ha fornito kernel_space");

  spinlock_lock(&vmm_lock);
  vmm_state.kernel_space = kspace;
  vmm_state.total_spaces_created = 1;
  vmm_state.total_mappings = 0;
  vmm_state.total_unmappings = 0;
  vmm_state.initialized = true;
  spinlock_unlock(&vmm_lock);

  klog_info("VMM: pronto (kernel_space=%p)", kspace);
}

vmm_space_t *vmm_kernel_space(void) {
  spinlock_lock(&vmm_lock);
  vmm_space_t *ks = vmm_state.initialized ? vmm_state.kernel_space : (vmm_space_t *)0;
  spinlock_unlock(&vmm_lock);
  return ks;
}

vmm_space_t *vmm_create_space(void) {
  spinlock_lock(&vmm_lock);
  if (!vmm_state.initialized) {
    spinlock_unlock(&vmm_lock);
    klog_error("VMM: create_space con VMM non inizializzato");
    return NULL;
  }
  spinlock_unlock(&vmm_lock);

  vmm_space_t *sp = arch_vmm_create_space();

  spinlock_lock(&vmm_lock);
  if (sp)
    vmm_state.total_spaces_created++;
  spinlock_unlock(&vmm_lock);

  return sp;
}

void vmm_destroy_space(vmm_space_t *space) {
  spinlock_lock(&vmm_lock);
  if (!validate_space_locked(space, "destroy_space")) {
    spinlock_unlock(&vmm_lock);
    return;
  }
  if (space == vmm_state.kernel_space) {
    spinlock_unlock(&vmm_lock);
    klog_error("VMM: tentata distruzione del kernel_space");
    return;
  }
  spinlock_unlock(&vmm_lock);

  arch_vmm_destroy_space(space);
}

void vmm_switch_space(vmm_space_t *space) {
  spinlock_lock(&vmm_lock);
  if (!validate_space_locked(space, "switch_space")) {
    spinlock_unlock(&vmm_lock);
    return;
  }
  spinlock_unlock(&vmm_lock);

  arch_vmm_switch_space(space);
}

bool vmm_map(vmm_space_t *space, u64 va, u64 pa, size_t pages, u64 flags) {
  spinlock_lock(&vmm_lock);
  if (!space)
    space = vmm_state.kernel_space;
  if (!validate_mapping_params_locked(space, va, pa, pages)) {
    spinlock_unlock(&vmm_lock);
    return false;
  }
  arch_vmm_flags_t af = vmm_to_arch_flags(flags);
  u64 ps = arch_memory_page_size();
  spinlock_unlock(&vmm_lock);

  /* Map per‑pagina con rollback su errore */
  size_t i = 0;
  for (; i < pages; ++i) {
    arch_vmm_res_t rc = arch_vmm_map(space, (void *)(va + i * ps), pa + i * ps, af);
    if (rc != ARCH_VMM_OK) {
      klog_error("VMM: map fallita alla pagina %zu (rc=%d), rollback", i, (int)rc);
      while (i > 0) {
        --i;
        (void)arch_vmm_unmap(space, (void *)(va + i * ps));
      }
      return false;
    }
  }

  spinlock_lock(&vmm_lock);
  vmm_state.total_mappings += pages;
  spinlock_unlock(&vmm_lock);
  return true;
}

void vmm_unmap(vmm_space_t *space, u64 va, size_t pages) {
  spinlock_lock(&vmm_lock);
  if (!space)
    space = vmm_state.kernel_space;
  if (!validate_space_locked(space, "unmap")) {
    spinlock_unlock(&vmm_lock);
    return;
  }
  if (!mm_is_page_aligned(va) || pages == 0) {
    klog_error("VMM: unmap parametri non validi (va=0x%lx pages=%zu)", va, pages);
    spinlock_unlock(&vmm_lock);
    return;
  }
  u64 ps = arch_memory_page_size();
  spinlock_unlock(&vmm_lock);

  for (size_t i = 0; i < pages; ++i) {
    (void)arch_vmm_unmap(space, (void *)(va + i * ps));
  }

  spinlock_lock(&vmm_lock);
  vmm_state.total_unmappings += pages;
  spinlock_unlock(&vmm_lock);
}

bool vmm_resolve(vmm_space_t *space, u64 va, u64 *out_pa) {
  spinlock_lock(&vmm_lock);
  if (!space)
    space = vmm_state.kernel_space;
  if (!validate_space_locked(space, "resolve")) {
    spinlock_unlock(&vmm_lock);
    return false;
  }
  spinlock_unlock(&vmm_lock);

  uint64_t pa = 0;
  bool ok = arch_vmm_resolve(space, va, &pa);
  if (ok && out_pa)
    *out_pa = pa;
  return ok;
}

/* -------------------------------------------------------------------------- */
/*                           WRAPPERS FISICO↔VIRTUALE                          */
/* -------------------------------------------------------------------------- */

void *vmm_phys_to_virt(u64 phys_addr) {
  return arch_vmm_phys_to_virt(phys_addr);
}

u64 vmm_virt_to_phys(u64 virt_addr) {
  return arch_vmm_virt_to_phys(virt_addr);
}
