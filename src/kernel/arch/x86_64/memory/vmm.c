/**
 * @file arch/x86_64/memory/vmm.c
 * @brief Backend VMM x86_64 per ZONE-OS (PML4, 4KiB pages)
 *
 * Implementa <arch/vmm.h> usando paging a 4 livelli (PML4→PDPT→PD→PT).
 * Alloca le strutture con il PMM e le manipola via HHDM (Limine).
 *
 * Funzionalità:
 *  - arch_vmm_init(): acquisisce HHDM offset e lascia CR3 corrente
 *  - arch_vmm_create_space()/destroy: crea/distrugge un PML4
 *  - arch_vmm_switch_space(): carica CR3
 *  - arch_vmm_map()/unmap(): map/unmap 4KiB con invlpg
 *
 * Note:
 *  - Supporta flag: READ/WRITE/USER/NOEXEC/GLOBAL. UC/WT parziali (PWT/PCD).
 *  - WC non supportato senza PAT → ritorna ARCH_VMM_EUNSUPPORTED.
 *  - Non gestisce large pages (2MiB/1GiB) in questa versione.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include <arch/memory.h>
#include <arch/vmm.h>
#include <klib/klog/klog.h>
#include <lib/stdbool.h>
#include <lib/stddef.h>
#include <lib/stdint.h>
#include <lib/string/string.h>
#include <limine.h>
#include <mm/heap/heap.h>
#include <mm/pmm.h>

/* Maschere indirizzo per large pages */
#define ADDR_1G_MASK ((uint64_t)0x000FFFFFC0000000ULL) /* 1GiB page base */
#define ADDR_2M_MASK ((uint64_t)0x000FFFFFFFE00000ULL) /* 2MiB page base */

/* Spazio kernel (derivato dal CR3 corrente) */
static vmm_space_t g_kernel_space;

/* ---------------------------------- HHDM ---------------------------------- */

static volatile struct limine_hhdm_request g_hhdm_req = {.id = LIMINE_HHDM_REQUEST, .revision = 0};
static uint64_t g_hhdm_off = 0;

static inline void *phys_to_virt(uint64_t phys) {
  return (void *)(phys + g_hhdm_off);
}

/* ------------------------------- CR3 / INVLPG ----------------------------- */

static inline uint64_t x86_read_cr3(void) {
  uint64_t v;
  __asm__ volatile("mov %%cr3,%0" : "=r"(v));
  return v;
}
static inline void x86_write_cr3(uint64_t phys) {
  __asm__ volatile("mov %0,%%cr3" ::"r"(phys) : "memory");
}
static inline void x86_invlpg(void *va) {
  __asm__ volatile("invlpg (%0)" ::"r"(va) : "memory");
}

/* ------------------------------- VMM structs ------------------------------ */

typedef uint64_t pte_t;

struct vmm_space {
  uint64_t pml4_phys; /* indirizzo fisico del PML4 */
};

/* ----------------------- Indici e bit entry pagine ------------------------ */

#define IDX_PML4(va) (((uint64_t)(va) >> 39) & 0x1FFULL)
#define IDX_PDPT(va) (((uint64_t)(va) >> 30) & 0x1FFULL)
#define IDX_PD(va) (((uint64_t)(va) >> 21) & 0x1FFULL)
#define IDX_PT(va) (((uint64_t)(va) >> 12) & 0x1FFULL)

#define PTE_P (1ULL << 0)
#define PTE_RW (1ULL << 1)
#define PTE_US (1ULL << 2)
#define PTE_PWT (1ULL << 3)
#define PTE_PCD (1ULL << 4)
#define PTE_A (1ULL << 5)
#define PTE_D (1ULL << 6)
#define PTE_PS (1ULL << 7)
#define PTE_G (1ULL << 8)
#define PTE_NX (1ULL << 63)

#define PTE_ADDR_MASK ((uint64_t)0x000FFFFFFFFFF000ULL)

/* --------------------------- Flag mapping helper -------------------------- */

static inline int map_flags_to_pte(arch_vmm_flags_t flags, uint64_t *out_pte_bits) {
  uint64_t bits = PTE_P; /* present by default (READ) */

  if (flags & ARCH_VMM_WRITE)
    bits |= PTE_RW;
  if (flags & ARCH_VMM_USER)
    bits |= PTE_US;
  if (flags & ARCH_VMM_NOEXEC)
    bits |= PTE_NX;
  if (flags & ARCH_VMM_GLOBAL)
    bits |= PTE_G;

  /* Caching policy (parziale): UC→PCD|PWT, WT→PWT, WC non supportato qui */
  if (flags & ARCH_VMM_UC)
    bits |= (PTE_PCD | PTE_PWT);
  if (flags & ARCH_VMM_WT)
    bits |= PTE_PWT;
  if (flags & ARCH_VMM_WC)
    return -1; /* richiede PAT: non gestito in questa versione */

  *out_pte_bits = bits;
  return 0;
}

/* ----------------------------- Walker tabelle ----------------------------- */

static inline pte_t *table_virt(uint64_t phys) {
  return (pte_t *)phys_to_virt(phys);
}

/* Crea (se necessario) un livello di tabella: setta P|RW e, se richiesto, US */
static inline uint64_t ensure_subtable(pte_t *tbl, size_t idx, bool need_user) {
  pte_t e = tbl[idx];
  if (e & PTE_P) {
    return (e & PTE_ADDR_MASK);
  }
  void *page = pmm_alloc_page();
  if (!page)
    return 0;
  uint64_t phys = (uint64_t)page;
  memset(phys_to_virt(phys), 0, arch_memory_page_size());
  uint64_t flags = PTE_P | PTE_RW;
  if (need_user)
    flags |= PTE_US;
  tbl[idx] = (phys & PTE_ADDR_MASK) | flags;
  return phys;
}

/* Ritorna puntatore al PTE leaf (PT entry) per 'virt'. Alloca tabelle se create=true. */
static pte_t *walk_get_pte(vmm_space_t *space, void *virt, bool create, bool need_user) {
  uint64_t pml4_phys = space->pml4_phys;
  if (!pml4_phys)
    return NULL;

  pte_t *pml4 = table_virt(pml4_phys);
  uint64_t pdpt_phys = pml4[IDX_PML4(virt)] & PTE_ADDR_MASK;
  if (!pdpt_phys) {
    if (!create)
      return NULL;
    pdpt_phys = ensure_subtable(pml4, IDX_PML4(virt), need_user);
    if (!pdpt_phys)
      return NULL;
  }

  pte_t *pdpt = table_virt(pdpt_phys);
  uint64_t pd_phys = pdpt[IDX_PDPT(virt)] & PTE_ADDR_MASK;
  if (!pd_phys) {
    if (!create)
      return NULL;
    pd_phys = ensure_subtable(pdpt, IDX_PDPT(virt), need_user);
    if (!pd_phys)
      return NULL;
  }

  pte_t *pd = table_virt(pd_phys);
  /* Verifica non large page */
  if (pd[IDX_PD(virt)] & PTE_PS)
    return NULL; /* non gestiamo 2MiB qui */

  uint64_t pt_phys = pd[IDX_PD(virt)] & PTE_ADDR_MASK;
  if (!pt_phys) {
    if (!create)
      return NULL;
    pt_phys = ensure_subtable(pd, IDX_PD(virt), need_user);
    if (!pt_phys)
      return NULL;
  }

  pte_t *pt = table_virt(pt_phys);
  return &pt[IDX_PT(virt)];
}

/* --------------------------------- API ------------------------------------ */

void arch_vmm_init(void) {
  if (!g_hhdm_req.response || !g_hhdm_req.response->offset) {
    klog_panic("x86_64/vmm: HHDM non disponibile (Limine)");
  }
  g_hhdm_off = g_hhdm_req.response->offset;
  /* Lasciamo CR3 così com’è: il bootstrap ha già un PML4 attivo */
  klog_info("x86_64/vmm: HHDM @ 0x%lx, CR3=0x%lx", g_hhdm_off, x86_read_cr3());
}

vmm_space_t *arch_vmm_create_space(void) {
  /* Metadati su heap kernel, non in memoria fisica */
  vmm_space_t *sp = (vmm_space_t *)kmalloc(sizeof(*sp));
  if (!sp)
    return NULL;
  memset(sp, 0, sizeof(*sp));

  /* PML4 su pagina fisica allocata dal PMM */
  void *pml4_page = pmm_alloc_page();
  if (!pml4_page) {
    kfree(sp);
    return NULL;
  }
  uint64_t pml4_phys = (uint64_t)pml4_page;

  /* Azzeriamo il nuovo PML4 */
  pte_t *new_pml4 = (pte_t *)phys_to_virt(pml4_phys);
  memset(new_pml4, 0, arch_memory_page_size());

  /* Condivisione higher-half kernel:
     Copia le voci [256..511] dal PML4 attivo per condividere lo spazio kernel. */
  uint64_t cur_pml4_phys = x86_read_cr3() & PTE_ADDR_MASK;
  pte_t *cur_pml4 = (pte_t *)phys_to_virt(cur_pml4_phys);
  for (size_t i = 256; i < 512; ++i) {
    new_pml4[i] = cur_pml4[i];
  }

  sp->pml4_phys = pml4_phys;
  return sp;
}

/* Libera ricorsivamente SOLO le strutture delle tabelle utente (lower-half).
   Non libera le pagine fisiche mappate dagli utenti: la policy di ownership
   resta al chiamante/VM manager; qui si rilasciano solo i page tables. */
void arch_vmm_destroy_space(vmm_space_t *space) {
  if (!space)
    return;
  if (space->pml4_phys) {
    pte_t *pml4 = (pte_t *)phys_to_virt(space->pml4_phys);

    /* Walk: PML4[0..255] (user half) */
    for (size_t i4 = 0; i4 < 256; ++i4) {
      pte_t e4 = pml4[i4];
      if (!(e4 & PTE_P))
        continue;
      uint64_t pdpt_phys = e4 & PTE_ADDR_MASK;
      pte_t *pdpt = (pte_t *)phys_to_virt(pdpt_phys);

      for (size_t i3 = 0; i3 < 512; ++i3) {
        pte_t e3 = pdpt[i3];
        if (!(e3 & PTE_P))
          continue;
        uint64_t pd_phys = e3 & PTE_ADDR_MASK;
        pte_t *pd = (pte_t *)phys_to_virt(pd_phys);

        for (size_t i2 = 0; i2 < 512; ++i2) {
          pte_t e2 = pd[i2];
          if (!(e2 & PTE_P))
            continue;

          /* Non gestiamo huge pages (PS) in questo backend: devono essere assenti */
          if (e2 & PTE_PS) {
            /* Se mai presente, NON liberare frame utente; libera solo la tabella PD dopo. */
            continue;
          }

          uint64_t pt_phys = e2 & PTE_ADDR_MASK;
          /* Liberiamo la PT (le sue PTE leaf non vengono “free-ate” dal PMM) */
          pmm_free_page((void *)pt_phys);
        }

        /* Liberiamo la PD */
        pmm_free_page((void *)pd_phys);
      }

      /* Liberiamo la PDPT */
      pmm_free_page((void *)pdpt_phys);
    }

    /* Infine libera il PML4 dello spazio */
    pmm_free_page((void *)space->pml4_phys);
  }
  kfree(space);
}

void arch_vmm_switch_space(vmm_space_t *space) {
  if (!space || !space->pml4_phys)
    return;
  x86_write_cr3(space->pml4_phys);
}

arch_vmm_res_t arch_vmm_map(vmm_space_t *space, void *virt, uint64_t phys, arch_vmm_flags_t flags) {
  if (!space || !space->pml4_phys)
    return ARCH_VMM_EINVAL;

  uint64_t ps = arch_memory_page_size();
  if (((uint64_t)virt % ps) || (phys % ps))
    return ARCH_VMM_EINVAL;

  uint64_t pte_bits;
  if (map_flags_to_pte(flags, &pte_bits) != 0)
    return ARCH_VMM_EUNSUPPORTED;

  bool need_user = (flags & ARCH_VMM_USER) != 0;
  pte_t *pte = walk_get_pte(space, virt, /*create*/ true, need_user);
  if (!pte)
    return ARCH_VMM_ENOMEM;

  if (*pte & PTE_P)
    return ARCH_VMM_EBUSY; /* già mappato */

  *pte = (phys & PTE_ADDR_MASK) | pte_bits;
  x86_invlpg(virt);
  return ARCH_VMM_OK;
}

arch_vmm_res_t arch_vmm_unmap(vmm_space_t *space, void *virt) {
  if (!space || !space->pml4_phys)
    return ARCH_VMM_EINVAL;

  uint64_t ps = arch_memory_page_size();
  if (((uint64_t)virt % ps) != 0)
    return ARCH_VMM_EINVAL;

  pte_t *pte = walk_get_pte(space, virt, /*create*/ false, /*need_user*/ false);
  if (!pte || !(*pte & PTE_P))
    return ARCH_VMM_ENOMAP;

  *pte = 0;
  x86_invlpg(virt);
  return ARCH_VMM_OK;
}

/**
 * @brief Risolve virtuale → fisico nello spazio dato (4K/2M/1G).
 */
arch_vmm_res_t arch_vmm_resolve(vmm_space_t *space, uint64_t virt, uint64_t *out_phys) {
  if (!space || !space->pml4_phys)
    return ARCH_VMM_EINVAL;

  pte_t *pml4 = table_virt(space->pml4_phys);
  pte_t e4 = pml4[IDX_PML4(virt)];
  if (!(e4 & PTE_P))
    return ARCH_VMM_ENOMAP;

  pte_t *pdpt = table_virt(e4 & PTE_ADDR_MASK);
  pte_t e3 = pdpt[IDX_PDPT(virt)];
  if (!(e3 & PTE_P))
    return ARCH_VMM_ENOMAP;

  /* 1 GiB page */
  if (e3 & PTE_PS) {
    uint64_t base = e3 & ADDR_1G_MASK;
    uint64_t off = virt & ((1ULL << 30) - 1);
    if (out_phys)
      *out_phys = base + off;
    return ARCH_VMM_OK;
  }

  pte_t *pd = table_virt(e3 & PTE_ADDR_MASK);
  pte_t e2 = pd[IDX_PD(virt)];
  if (!(e2 & PTE_P))
    return ARCH_VMM_ENOMAP;

  /* 2 MiB page */
  if (e2 & PTE_PS) {
    uint64_t base = e2 & ADDR_2M_MASK;
    uint64_t off = virt & ((1ULL << 21) - 1);
    if (out_phys)
      *out_phys = base + off;
    return ARCH_VMM_OK;
  }

  pte_t *pt = table_virt(e2 & PTE_ADDR_MASK);
  pte_t e1 = pt[IDX_PT(virt)];
  if (!(e1 & PTE_P))
    return ARCH_VMM_ENOMAP;

  if (out_phys)
    *out_phys = (e1 & PTE_ADDR_MASK) | (virt & 0xFFFULL);
  return ARCH_VMM_OK;
}

uint64_t arch_vmm_virt_to_phys(uint64_t virt) {
  /* Walk sulla page table corrente (CR3) via HHDM, no allocazioni. */
  const uint64_t cr3_phys = x86_read_cr3() & PTE_ADDR_MASK;
  if (!cr3_phys)
    return 0;

  pte_t *pml4 = table_virt(cr3_phys);
  pte_t e4 = pml4[IDX_PML4(virt)];
  if (!(e4 & PTE_P))
    return 0;

  pte_t *pdpt = table_virt(e4 & PTE_ADDR_MASK);
  pte_t e3 = pdpt[IDX_PDPT(virt)];
  if (!(e3 & PTE_P))
    return 0;

  /* 1GiB page? (PS in PDPTE) */
  if (e3 & PTE_PS) {
    uint64_t base = e3 & ADDR_1G_MASK;
    uint64_t off = virt & ((1ULL << 30) - 1);
    return base + off;
  }

  pte_t *pd = table_virt(e3 & PTE_ADDR_MASK);
  pte_t e2 = pd[IDX_PD(virt)];
  if (!(e2 & PTE_P))
    return 0;

  /* 2MiB page? (PS in PDE) */
  if (e2 & PTE_PS) {
    uint64_t base = e2 & ADDR_2M_MASK;
    uint64_t off = virt & ((1ULL << 21) - 1);
    return base + off;
  }

  pte_t *pt = table_virt(e2 & PTE_ADDR_MASK);
  pte_t e1 = pt[IDX_PT(virt)];
  if (!(e1 & PTE_P))
    return 0;

  /* 4KiB page */
  return (e1 & PTE_ADDR_MASK) | (virt & 0xFFFULL);
}

void *arch_vmm_phys_to_virt(uint64_t phys) {
  /* Mapping HHDM: vaddr = phys + g_hhdm_off (inizializzato in arch_vmm_init). */
  return phys_to_virt(phys);
}

/**
 * @brief Restituisce lo spazio di indirizzamento del kernel.
 */
vmm_space_t *arch_vmm_get_kernel_space(void) {
  g_kernel_space.pml4_phys = x86_read_cr3() & PTE_ADDR_MASK;
  return &g_kernel_space;
}