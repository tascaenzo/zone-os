/**
 * @file include/arch/vmm.h
 * @brief API VMM portabile: gestione spazi di indirizzamento e mappature
 *
 * Estensione dell'interfaccia astratta per la paginazione: include operazioni
 * su range, gestione pagine grandi, aggiornamento permessi, query dettagliata
 * delle PTE e funzioni di manutenzione TLB. Tutte le funzioni qui definite
 * operano a livello di meccanismo architetturale, senza introdurre logiche
 * di policy proprie del kernel.
 *
 * Ogni architettura implementa queste API in src/arch/<arch>/vmm.c.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once

#include <lib/stdbool.h>
#include <lib/stddef.h>
#include <lib/stdint.h>

/* Opaque handle allo spazio di paginazione (PML4 su x86_64, ecc.) */
typedef struct vmm_space vmm_space_t;

/**
 * @brief Codici di ritorno per operazioni VMM.
 */
typedef enum {
  ARCH_VMM_OK = 0,      /**< Operazione completata */
  ARCH_VMM_EINVAL,      /**< Parametri non validi / non allineati */
  ARCH_VMM_ENOMEM,      /**< Memoria insufficiente per tabelle */
  ARCH_VMM_ENOMAP,      /**< Nessuna mappatura presente da rimuovere */
  ARCH_VMM_EBUSY,       /**< Risorsa occupata (es. double map non consentita) */
  ARCH_VMM_EUNSUPPORTED /**< Operazione/flag non supportati dalla arch */
} arch_vmm_res_t;

/**
 * @brief Flag di mappatura generici (tradotti dal backend arch).
 */
typedef uint64_t arch_vmm_flags_t;
#define ARCH_VMM_READ (1ull << 0)   /**< Lettura consentita */
#define ARCH_VMM_WRITE (1ull << 1)  /**< Scrittura consentita */
#define ARCH_VMM_USER (1ull << 2)   /**< Accessibile in ring utente */
#define ARCH_VMM_NOEXEC (1ull << 3) /**< Esecuzione vietata (NX) */
#define ARCH_VMM_GLOBAL (1ull << 4) /**< Global TLB (se supportato) */
#define ARCH_VMM_WC (1ull << 5)     /**< Write-Combining (se supportato) */
#define ARCH_VMM_WT (1ull << 6)     /**< Write-Through (se supportato) */
#define ARCH_VMM_UC (1ull << 7)     /**< Uncacheable (se supportato) */

/* Flag per pagine grandi (se supportate dall’arch) */
#define ARCH_VMM_PS_4K (0ull)      /**< Pagina da 4 KiB */
#define ARCH_VMM_PS_2M (1ull << 8) /**< Pagina da 2 MiB */
#define ARCH_VMM_PS_1G (1ull << 9) /**< Pagina da 1 GiB */

/**
 * @brief Inizializza il backend VMM dell’architettura.
 */
void arch_vmm_init(void);

/**
 * @brief Restituisce lo spazio di indirizzamento del kernel.
 */
vmm_space_t *arch_vmm_get_kernel_space(void);

/**
 * @brief Crea un nuovo address space vuoto.
 */
vmm_space_t *arch_vmm_create_space(void);

/**
 * @brief Distrugge uno spazio precedentemente creato.
 */
void arch_vmm_destroy_space(vmm_space_t *space);

/**
 * @brief Attiva lo spazio dato (switch CR3 o equivalente).
 */
void arch_vmm_switch_space(vmm_space_t *space);

/**
 * @brief Mappa una singola pagina fisica a un indirizzo virtuale.
 *
 * @param space  Spazio target (non NULL)
 * @param virt   Indirizzo virtuale allineato a pagina
 * @param phys   Indirizzo fisico allineato a pagina
 * @param flags  Flag ARCH_VMM_* (inclusi eventuali ARCH_VMM_PS_* se supportati)
 */
arch_vmm_res_t arch_vmm_map(vmm_space_t *space, void *virt, uint64_t phys, arch_vmm_flags_t flags);

/**
 * @brief Rimuove la mappatura di una singola pagina virtuale.
 */
arch_vmm_res_t arch_vmm_unmap(vmm_space_t *space, void *virt);

/**
 * @brief Mappa un intervallo contiguo di pagine fisiche.
 *
 * @param space       Spazio target
 * @param virt_start  Indirizzo virtuale iniziale (allineato a pagina)
 * @param phys_start  Indirizzo fisico iniziale (allineato a pagina)
 * @param page_count  Numero di pagine da mappare
 * @param flags       Flag ARCH_VMM_* (inclusi eventuali ARCH_VMM_PS_*)
 */
arch_vmm_res_t arch_vmm_map_range(vmm_space_t *space, void *virt_start, uint64_t phys_start, size_t page_count, arch_vmm_flags_t flags);

/**
 * @brief Rimuove un intervallo contiguo di pagine virtuali.
 *
 * @param space       Spazio target
 * @param virt_start  Indirizzo virtuale iniziale
 * @param page_count  Numero di pagine da rimuovere
 */
arch_vmm_res_t arch_vmm_unmap_range(vmm_space_t *space, void *virt_start, size_t page_count);

/**
 * @brief Aggiorna i permessi di una pagina mappata.
 *
 * @param space  Spazio target
 * @param virt   Indirizzo virtuale
 * @param flags  Nuovi flag ARCH_VMM_*
 */
arch_vmm_res_t arch_vmm_protect(vmm_space_t *space, void *virt, arch_vmm_flags_t flags);

/**
 * @brief Aggiorna i permessi di un intervallo di pagine mappate.
 *
 * @param space       Spazio target
 * @param virt_start  Indirizzo virtuale iniziale
 * @param page_count  Numero di pagine
 * @param flags       Nuovi flag ARCH_VMM_*
 */
arch_vmm_res_t arch_vmm_protect_range(vmm_space_t *space, void *virt_start, size_t page_count, arch_vmm_flags_t flags);

/**
 * @brief Struttura con informazioni dettagliate su una PTE.
 */
typedef struct {
  bool present;               /**< Pagina presente in memoria */
  bool writable;              /**< Scrivibile */
  bool user;                  /**< Accessibile da ring utente */
  bool noexec;                /**< Esecuzione vietata */
  bool global;                /**< Mappatura globale */
  bool accessed;              /**< Bit accessed (se supportato) */
  bool dirty;                 /**< Bit dirty (se supportato) */
  uint8_t page_shift;         /**< log2(size) → 12=4K, 21=2M, 30=1G */
  arch_vmm_flags_t eff_flags; /**< Flag effettivi tradotti */
  uint64_t phys_page_base;    /**< Base fisica della pagina */
} arch_vmm_pte_info_t;

/**
 * @brief Risolve virtuale → fisico nello spazio dato.
 */
arch_vmm_res_t arch_vmm_resolve(vmm_space_t *space, uint64_t virt, uint64_t *out_phys);

/**
 * @brief Restituisce informazioni dettagliate sulla PTE corrispondente.
 *
 * @param space  Spazio target
 * @param virt   Indirizzo virtuale
 * @param out    Struttura di output (non NULL)
 */
arch_vmm_res_t arch_vmm_query(vmm_space_t *space, uint64_t virt, arch_vmm_pte_info_t *out);

/**
 * @brief Restituisce i page shift supportati dall'architettura.
 *
 * @param out_shifts  Puntatore a buffer statico di proprietà del backend
 * @return Numero di valori disponibili in out_shifts
 */
size_t arch_vmm_supported_pageshifts(const uint8_t **out_shifts);

/**
 * @brief Svuota l'intero TLB per lo spazio dato.
 */
void arch_vmm_flush_tlb_space(vmm_space_t *space);

/**
 * @brief Svuota la voce TLB relativa a una singola pagina.
 */
void arch_vmm_flush_tlb_page(vmm_space_t *space, void *virt);

/**
 * @brief Svuota le voci TLB relative a un intervallo di pagine.
 */
void arch_vmm_flush_tlb_range(vmm_space_t *space, void *virt_start, size_t page_count);

/**
 * @brief Sincronizza gli aggiornamenti alle page table (barriera).
 */
void arch_vmm_pt_sync(void);

/**
 * @brief Mappa temporaneamente una pagina fisica nello spazio kernel.
 *
 * @param phys  Indirizzo fisico (allineato a pagina)
 * @return Indirizzo virtuale temporaneo
 */
void *arch_vmm_kmap_temp(uint64_t phys);

/**
 * @brief Rimuove una mappatura temporanea precedentemente creata.
 */
void arch_vmm_kunmap_temp(void *virt);

/**
 * @brief Converte fisico → virtuale secondo il modello dell’arch (es. HHDM).
 */
void *arch_vmm_phys_to_virt(uint64_t phys);

/**
 * @brief Converte virtuale → fisico (se mappato nello spazio corrente).
 */
uint64_t arch_vmm_virt_to_phys(uint64_t virt);
