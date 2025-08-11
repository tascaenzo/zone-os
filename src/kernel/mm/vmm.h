/**
 * @file mm/vmm.h
 * @brief Virtual Memory Manager (arch‑agnostico)
 *
 * API di alto livello per gestione spazi di indirizzamento e mappature.
 * Il backend architetturale è fornito da <arch/vmm.h>. Questo header
 * non espone dettagli di page table specifici.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once

#include <arch/memory.h>
#include <lib/stdbool.h>
#include <lib/stddef.h>
#include <lib/types.h>

/* -------------------------------------------------------------------------- */
/*                              CONFIGURAZIONE                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Limite indicativo per operazioni di mapping massivo (pagine).
 *        Usato per log/diagnostica; non è un vincolo hard.
 */
#define VMM_MAX_MAPPING_PAGES (1UL << 20)

/* -------------------------------------------------------------------------- */
/*                                   TIPI                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief Flag generici per mappature virtuali.
 *        Saranno tradotti dal core verso i flag arch‑specifici.
 */
typedef enum {
  VMM_FLAG_READ = (1u << 0),     /**< Lettura consentita            */
  VMM_FLAG_WRITE = (1u << 1),    /**< Scrittura consentita          */
  VMM_FLAG_EXEC = (1u << 2),     /**< Esecuzione consentita         */
  VMM_FLAG_USER = (1u << 3),     /**< Accessibile da ring utente    */
  VMM_FLAG_GLOBAL = (1u << 4),   /**< Global TLB (se supportato)    */
  VMM_FLAG_NO_CACHE = (1u << 5), /**< Uncacheable/WT/PCD+PWT best‑effort */
} vmm_flags_t;

/**
 * @brief Tipo opaco per uno spazio di indirizzamento.
 */
typedef struct vmm_space vmm_space_t;

/* -------------------------------------------------------------------------- */
/*                               INIZIALIZZAZIONE                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Inizializza il VMM globale (backend arch incluso).
 */
void vmm_init(void);

/**
 * @brief Restituisce lo spazio di indirizzamento del kernel.
 */
vmm_space_t *vmm_kernel_space(void);

/* -------------------------------------------------------------------------- */
/*                        GESTIONE ADDRESS SPACE (PROCESSI)                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Crea un nuovo spazio virtuale (es. processo).
 * @return Nuovo spazio o NULL su errore.
 */
vmm_space_t *vmm_create_space(void);

/**
 * @brief Distrugge uno spazio virtuale e libera le strutture di paging.
 * @param space Spazio da distruggere.
 */
void vmm_destroy_space(vmm_space_t *space);

/**
 * @brief Rende attivo lo spazio indicato (switch CR3 o equivalente).
 * @param space Spazio da attivare.
 */
void vmm_switch_space(vmm_space_t *space);

/* -------------------------------------------------------------------------- */
/*                             MAP / UNMAP / RESOLVE                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Mappa un range fisico in uno spazio virtuale.
 * @param space      Spazio target (NULL = spazio corrente).
 * @param virt_addr  Virtuale base (allineato a pagina).
 * @param phys_addr  Fisico base (allineato a pagina).
 * @param page_count Numero di pagine da mappare (>0).
 * @param flags      Combinazione di VMM_FLAG_*.
 * @return true su successo, false altrimenti.
 */
bool vmm_map(vmm_space_t *space, u64 virt_addr, u64 phys_addr, size_t page_count, u64 flags);

/**
 * @brief Smappa un range virtuale.
 * @param space      Spazio target (NULL = spazio corrente).
 * @param virt_addr  Virtuale base (allineato a pagina).
 * @param page_count Numero di pagine da smappare (>0).
 */
void vmm_unmap(vmm_space_t *space, u64 virt_addr, size_t page_count);

/**
 * @brief Risolve virtuale→fisico.
 * @param space        Spazio target.
 * @param virt_addr    Indirizzo virtuale.
 * @param out_phys_addr (opzionale) buffer per il fisico.
 * @return true se esiste una mappatura valida.
 */
bool vmm_resolve(vmm_space_t *space, u64 virt_addr, u64 *out_phys_addr);

/* -------------------------------------------------------------------------- */
/*                           CONVERSIONI COMODE (KERNEL)                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Converte fisico→virtuale nello spazio kernel (es. HHDM).
 * @param phys_addr Indirizzo fisico.
 * @return Virtuale corrispondente.
 */
void *vmm_phys_to_virt(u64 phys_addr);

/**
 * @brief Converte virtuale→fisico nello spazio corrente.
 * @param virt_addr Indirizzo virtuale.
 * @return Indirizzo fisico (0 se non mappato).
 */
u64 vmm_virt_to_phys(u64 virt_addr);
