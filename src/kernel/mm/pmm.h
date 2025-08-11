/**
 * @file mm/pmm.h
 * @brief Physical Memory Manager - Architecture Agnostic
 *
 * Gestisce la memoria fisica in unità di pagina. Non dipende da header
 * privati dell’architettura; l’inizializzazione recupera la page size
 * tramite HAL (<arch/memory.h>) nel file .c, non in questo header.
 *
 * Autore: Enzo Tasca
 * @date 2025
 */

#pragma once

#include <lib/stdbool.h>
#include <lib/stddef.h>
#include <lib/types.h>

/**
 * @brief Limite difensivo per richieste di allocazione (in pagine).
 */
#define PMM_MAX_REASONABLE_ALLOC_PAGES (1UL << 20) /* ~4 GiB @4KiB */

/**
 * @brief Risultati delle operazioni PMM.
 */
typedef enum { PMM_SUCCESS = 0, PMM_OUT_OF_MEMORY, PMM_INVALID_ADDRESS, PMM_ALREADY_FREE, PMM_NOT_INITIALIZED } pmm_result_t;

/**
 * @brief Statistiche del PMM.
 */
typedef struct {
  u64 total_pages;
  u64 free_pages;
  u64 used_pages;
  u64 reserved_pages;
  u64 bitmap_pages;
  u64 alloc_count;
  u64 free_count;
  u64 largest_free_run;
} pmm_stats_t;

/**
 * @brief Inizializza il Physical Memory Manager.
 *
 * @return PMM_SUCCESS se inizializzato correttamente.
 */
pmm_result_t pmm_init(void);

/**
 * @brief Alloca una singola pagina fisica.
 *
 * @return Indirizzo fisico allineato alla page size, o NULL se fallisce.
 */
void *pmm_alloc_page(void);

/**
 * @brief Alloca più pagine fisiche contigue.
 *
 * @param count Numero di pagine da allocare.
 * @return Base fisica del blocco, o NULL se fallisce.
 */
void *pmm_alloc_pages(size_t count);

/**
 * @brief Libera una singola pagina fisica.
 *
 * @param page Indirizzo fisico della pagina.
 * @return Codice risultato dell’operazione.
 */
pmm_result_t pmm_free_page(void *page);

/**
 * @brief Libera più pagine fisiche contigue.
 *
 * @param pages Base fisica del blocco.
 * @param count Numero di pagine da liberare.
 * @return Codice risultato dell’operazione.
 */
pmm_result_t pmm_free_pages(void *pages, size_t count);

/**
 * @brief Verifica se una pagina è libera.
 *
 * @param page Indirizzo fisico della pagina.
 * @return true se libera, false altrimenti.
 */
bool pmm_is_page_free(void *page);

/**
 * @brief Restituisce le statistiche correnti del PMM.
 *
 * @return Puntatore costante alle statistiche, o NULL se non inizializzato.
 */
const pmm_stats_t *pmm_get_stats(void);

/**
 * @brief Ottiene informazioni su una pagina.
 *
 * @param page Indirizzo fisico della pagina.
 * @param page_index Indice pagina (out, opzionale).
 * @param is_free Stato libero/occupato (out, opzionale).
 * @return true se valido, false altrimenti.
 */
bool pmm_get_page_info(void *page, u64 *page_index, bool *is_free);

/**
 * @brief Stampa informazioni diagnostiche del PMM.
 */
void pmm_print_info(void);

/**
 * @brief Verifica l’integrità del bitmap interno.
 *
 * @return true se consistente, false altrimenti.
 */
bool pmm_check_integrity(void);

/**
 * @brief Valida rapidamente le statistiche interne.
 *
 * @return true se coerenti, false altrimenti.
 */
bool pmm_validate_stats(void);

/**
 * @brief Trova il run contiguo di pagine libere più lungo.
 *
 * @param start_page Indice della prima pagina del run (out, opzionale).
 * @return Lunghezza del run in pagine.
 */
size_t pmm_find_largest_free_run(size_t *start_page);

/**
 * @brief Restituisce la dimensione della pagina usata dal PMM (byte).
 *
 * @return Dimensione di pagina in byte.
 */
u64 pmm_get_page_size(void);
