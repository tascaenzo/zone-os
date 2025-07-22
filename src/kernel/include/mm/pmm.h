#pragma once
#include <arch/x86_64/paging.h>
#include <lib/types.h>

/**
 * @file pmm.h
 * @brief Physical Memory Manager - Gestione pagine fisiche
 *
 * Il PMM gestisce la memoria fisica in blocchi di dimensione PAGE_SIZE.
 * Utilizza un bitmap per tracciare lo stato di ogni pagina.
 *
 * NOTA: Le costanti PAGE_SIZE, PAGE_SHIFT sono definite in arch/x86_64/paging.h
 */

// Risultati delle operazioni PMM
typedef enum { PMM_SUCCESS = 0, PMM_OUT_OF_MEMORY, PMM_INVALID_ADDRESS, PMM_ALREADY_FREE, PMM_NOT_INITIALIZED } pmm_result_t;

// Statistiche del PMM
typedef struct {
  u64 total_pages;    // Totale pagine gestite dal PMM
  u64 free_pages;     // Pagine attualmente libere
  u64 used_pages;     // Pagine attualmente occupate
  u64 reserved_pages; // Pagine riservate (kernel, hardware, etc.)
  u64 bitmap_pages;   // Pagine usate dal bitmap stesso

  // Statistiche di utilizzo
  u64 alloc_count;      // Numero totale di allocazioni
  u64 free_count;       // Numero totale di deallocazioni
  u64 largest_free_run; // Sequenza più lunga di pagine libere contigue
} pmm_stats_t;

/**
 * @brief Inizializza il Physical Memory Manager
 *
 * Analizza la memory map, crea il bitmap e marca le pagine
 * secondo il loro stato (libere, occupate, riservate).
 *
 * @return PMM_SUCCESS se l'inizializzazione è riuscita
 */
pmm_result_t pmm_init(void);

/**
 * @brief Alloca una singola pagina fisica (4KB)
 *
 * @return Indirizzo fisico della pagina allocata, o NULL se fallisce
 */
void *pmm_alloc_page(void);

/**
 * @brief Alloca multiple pagine fisiche contigue
 *
 * @param count Numero di pagine da allocare
 * @return Indirizzo fisico della prima pagina, o NULL se fallisce
 */
void *pmm_alloc_pages(size_t count);

/**
 * @brief Libera una singola pagina fisica
 *
 * @param page Indirizzo fisico della pagina da liberare
 * @return PMM_SUCCESS se liberata, errore altrimenti
 */
pmm_result_t pmm_free_page(void *page);

/**
 * @brief Libera multiple pagine fisiche contigue
 *
 * @param pages Indirizzo fisico della prima pagina
 * @param count Numero di pagine da liberare
 * @return PMM_SUCCESS se liberate, errore altrimenti
 */
pmm_result_t pmm_free_pages(void *pages, size_t count);

/**
 * @brief Verifica se una pagina è libera
 *
 * @param page Indirizzo fisico della pagina
 * @return true se libera, false se occupata o invalida
 */
bool pmm_is_page_free(void *page);

/**
 * @brief Ottiene le statistiche correnti del PMM
 *
 * @return Puntatore alla struttura delle statistiche
 */
const pmm_stats_t *pmm_get_stats(void);

/**
 * @brief Stampa informazioni dettagliate sullo stato del PMM
 */
void pmm_print_info(void);

/**
 * @brief Verifica l'integrità del bitmap (debug)
 *
 * Controlla che il bitmap sia consistente e che non ci siano
 * corruzioni o doppi utilizzi.
 *
 * @return true se il bitmap è integro
 */
bool pmm_check_integrity(void);

/**
 * @brief Trova la sequenza più lunga di pagine libere contigue
 *
 * Utile per statistiche e per allocazioni che richiedono
 * grandi blocchi contigui.
 *
 * @param start_page Puntatore per salvare l'indice della prima pagina
 * @return Numero di pagine nella sequenza più lunga
 */
size_t pmm_find_largest_free_run(size_t *start_page);

/**
 * @brief Alloca pagine in una zona specifica di memoria
 *
 * Utile per allocazioni che devono essere in zone particolari
 * (es: sotto 16MB per DMA legacy).
 *
 * @param count Numero di pagine da allocare
 * @param min_addr Indirizzo fisico minimo
 * @param max_addr Indirizzo fisico massimo
 * @return Indirizzo fisico della prima pagina, o NULL se fallisce
 */
void *pmm_alloc_pages_in_range(size_t count, u64 min_addr, u64 max_addr);

/**
 * @brief Alloca pagine con allineamento specifico
 *
 * @param pages Numero di pagine da allocare
 * @param alignment Allineamento richiesto in byte (deve essere potenza di 2)
 * @return Indirizzo fisico allineato, o NULL se fallisce
 */
void *pmm_alloc_aligned(size_t pages, size_t alignment);

/**
 * @brief Ottiene informazioni su una specifica pagina
 *
 * @param page Indirizzo della pagina
 * @param page_index Puntatore per salvare l'indice della pagina
 * @param is_free Puntatore per salvare se la pagina è libera
 * @return true se le informazioni sono valide
 */
bool pmm_get_page_info(void *page, u64 *page_index, bool *is_free);

/**
 * @brief Stampa statistiche di frammentazione dettagliate
 */
void pmm_print_fragmentation_info(void);