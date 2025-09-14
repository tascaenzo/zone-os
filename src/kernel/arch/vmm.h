// /**
//  * @file    arch/vmm.h
//  * @brief   API VMM arch-agnostica esposta al kernel (paging/MMU/TLB)
//  *
//  * Implementazione per-arch in: arch/<arch>/vmm/vmm_arch.c
//  * Interfaccia senza dipendenze da dettagli specifici di architettura.
//  *
//  * @author Enzo Tasca
//  * @date 2025
//  */
//
// #pragma once
//
// #include <lib/stdbool.h> /* bool     */
// #include <lib/stddef.h>  /* size_t   */
// #include <lib/stdint.h>  /* uint64_t */
// #include <lib/types.h>   /* u64      */
//
// /* Handle opaco allo spazio di indirizzamento */
// typedef struct vmm_space vmm_space_t;
//
// /* Bitmask flag definiti dal core (mm) — qui transitano senza semantica arch */
// typedef u64 vmm_flags_t;
//
// /* ============================================================================
//  *  INIT / CAPABILITIES
//  * ==========================================================================*/
//
// /**
//  * @brief Inizializza il sottosistema VMM (abilita feature disponibili, setup mode).
//  *        Va chiamata prima di qualsiasi altra API VMM.
//  *
//  * @return 0 su successo; codice negativo (-errno) su errore irreversibile.
//  */
// int vmm_init(void);
//
// /**
//  * @brief Restituisce la dimensione della pagina nativa dell’architettura.
//  *
//  * @return Dimensione pagina in byte (es. 4096).
//  */
// size_t vmm_page_size(void);
//
// /**
//  * @brief True se l’hardware supporta ed ha attivato il controllo di esecuzione per pagina.
//  *        Valido solo dopo vmm_init().
//  */
// bool vmm_has_exec_protection(void);
//
// /**
//  * @brief Verifica che un indirizzo virtuale sia canonico/valido secondo l’architettura.
//  *        Non accede alla memoria, è un controllo di formato.
//  *
//  * @param va Indirizzo virtuale da verificare.
//  * @return true se valido, false altrimenti.
//  */
// bool vmm_is_canonical(u64 va);
//
// /* ============================================================================
//  *  ADDRESS SPACE MANAGEMENT
//  * ==========================================================================*/
//
// /**
//  * @brief Restituisce lo spazio di indirizzamento del kernel.
//  *
//  * @return Puntatore allo spazio singleton del kernel.
//  */
// vmm_space_t *vmm_kernel_space(void);
//
// /**
//  * @brief Crea un nuovo spazio di indirizzamento.
//  *
//  * @return Puntatore al nuovo spazio, oppure NULL su errore.
//  */
// vmm_space_t *vmm_space_create(void);
//
// /**
//  * @brief Distrugge uno spazio di indirizzamento.
//  *        Non libera le pagine fisiche mappate.
//  *
//  * @param space Spazio da distruggere (non NULL).
//  */
// void vmm_space_destroy(vmm_space_t *space);
//
// /**
//  * @brief Attiva lo spazio di indirizzamento specificato.
//  *
//  * @param space Spazio da attivare (non NULL).
//  */
// void vmm_space_switch(vmm_space_t *space);
//
// /* ============================================================================
//  *  MAP / UNMAP / PROTECT / RESOLVE
//  * ==========================================================================*/
//
// /**
//  * @brief Mappa pagine contigue VA→PA con i flag specificati.
//  *
//  * @param space     Spazio di destinazione.
//  * @param virt      Indirizzo virtuale base (allineato a vmm_page_size()).
//  * @param phys      Indirizzo fisico base (allineato a vmm_page_size()).
//  * @param pages     Numero di pagine da mappare (>0).
//  * @param flags     Flag generici definiti dal core.
//  * @return 0 su successo; codice negativo (-errno) su errore.
//  */
// int vmm_map_pages(vmm_space_t *space, u64 virt, u64 phys, size_t pages, vmm_flags_t flags);
//
// /**
//  * @brief Smappa pagine contigue a partire da un indirizzo virtuale.
//  *        Non libera la memoria fisica.
//  *
//  * @param space Spazio di destinazione.
//  * @param virt  Indirizzo virtuale base.
//  * @param pages Numero di pagine da smappare.
//  * @return 0 su successo; codice negativo (-errno) su errore.
//  */
// int vmm_unmap_pages(vmm_space_t *space, u64 virt, size_t pages);
//
// /**
//  * @brief Aggiorna i permessi di pagine contigue già mappate.
//  *
//  * @param space     Spazio di destinazione.
//  * @param virt      Indirizzo virtuale base.
//  * @param pages     Numero di pagine.
//  * @param new_flags Nuovi flag generici.
//  * @return 0 su successo; codice negativo (-errno) su errore.
//  */
// int vmm_protect_pages(vmm_space_t *space, u64 virt, size_t pages, vmm_flags_t new_flags);
//
// /**
//  * @brief Risolve VA→PA se presente.
//  *
//  * @param space     Spazio da interrogare.
//  * @param virt      Indirizzo virtuale.
//  * @param out_phys  (opzionale) Riceve il PA se non NULL.
//  * @return true se l’indirizzo è mappato, false altrimenti.
//  */
// bool vmm_resolve(vmm_space_t *space, u64 virt, u64 *out_phys);
//
// /* ============================================================================
//  *  TLB OPERATIONS
//  * ==========================================================================*/
//
// /**
//  * @brief Invalida l’intero TLB (best-effort secondo architettura).
//  */
// void vmm_tlb_flush_all(void);
//
// /**
//  * @brief Invalida una singola pagina dal TLB.
//  *
//  * @param virt Indirizzo virtuale della pagina da invalidare.
//  */
// void vmm_tlb_invlpg(u64 virt);
//