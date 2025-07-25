#pragma once

#include <arch/memory.h>
#include <lib/types.h>

/* -------------------------------------------------------------------------- */
/*                         PMM CONFIGURATION CONSTANTS                        */
/* -------------------------------------------------------------------------- */

/*
 * Limite indicativo oltre il quale le richieste di allocazione vengono
 * considerate sospette (1M pagine = 4GB). Utilizzato per diagnosticare bug
 * o richieste anomale.
 */
#define PMM_MAX_REASONABLE_ALLOC_PAGES (1UL << 20)

/**
 * @file mm/pmm.h
 * @brief Physical Memory Manager - Architecture Agnostic
 *
 * Il PMM gestisce la memoria fisica in blocchi di dimensione PAGE_SIZE.
 * Utilizza un bitmap per tracciare lo stato di ogni pagina.
 *
 * DESIGN PRINCIPLES:
 * - Completamente independente dall'architettura
 * - Non dipende da bootloader specifici
 * - Interfaccia pulita con il layer architetturale via arch/memory.h
 * - Gestisce SOLO la logica del bitmap e allocazione pagine
 *
 * DEPENDENCIES:
 * - arch/memory.h: per tipi e costanti architetturali (PAGE_SIZE, etc.)
 * - Implementazione arch-specifica in arch/x86_64/memory.c
 */

/*
 * ============================================================================
 * TYPES AND CONSTANTS
 * ============================================================================
 */

/**
 * @brief Risultati delle operazioni PMM
 */
typedef enum {
  PMM_SUCCESS = 0,     /* Operazione completata con successo */
  PMM_OUT_OF_MEMORY,   /* Memoria fisica esaurita */
  PMM_INVALID_ADDRESS, /* Indirizzo non valido o non allineato */
  PMM_ALREADY_FREE,    /* Tentativo di liberare pagina già libera */
  PMM_NOT_INITIALIZED  /* PMM non ancora inizializzato */
} pmm_result_t;

/**
 * @brief Statistiche del PMM
 *
 * Fornisce informazioni dettagliate sullo stato corrente
 * del memory manager fisico.
 */
typedef struct {
  u64 total_pages;    /* Totale pagine gestite dal PMM */
  u64 free_pages;     /* Pagine attualmente libere */
  u64 used_pages;     /* Pagine attualmente occupate */
  u64 reserved_pages; /* Pagine riservate (kernel, hardware, etc.) */
  u64 bitmap_pages;   /* Pagine usate dal bitmap stesso */

  /* Statistiche di utilizzo */
  u64 alloc_count;      /* Numero totale di allocazioni */
  u64 free_count;       /* Numero totale di deallocazioni */
  u64 largest_free_run; /* Sequenza più lunga di pagine libere contigue */
} pmm_stats_t;

/*
 * ============================================================================
 * CORE PMM API
 * ============================================================================
 */

/**
 * @brief Inizializza il Physical Memory Manager
 *
 * PROCESSO:
 * 1. Inizializza il layer architetturale (arch_memory_init)
 * 2. Rileva le regioni di memoria tramite arch_memory_detect_regions
 * 3. Calcola dimensioni e alloca il bitmap
 * 4. Inizializza il bitmap secondo i tipi di regione
 * 5. Imposta lo stato interno del PMM
 *
 * @return PMM_SUCCESS se l'inizializzazione è riuscita
 * @return PMM_NOT_INITIALIZED se il layer arch non funziona
 * @return PMM_OUT_OF_MEMORY se non c'è spazio per il bitmap
 *
 * @note Questa funzione deve essere chiamata DOPO l'inizializzazione
 *       del sistema di logging e PRIMA di qualsiasi altra operazione PMM
 */
pmm_result_t pmm_init(void);

/*
 * ============================================================================
 * MEMORY ALLOCATION API
 * ============================================================================
 */

/**
 * @brief Alloca una singola pagina fisica
 *
 * Trova la prima pagina libera disponibile e la marca come occupata.
 * Usa un hint interno per ottimizzare ricerche successive.
 *
 * @return Indirizzo fisico della pagina allocata (allineato a PAGE_SIZE)
 * @return NULL se allocazione fallisce (memoria esaurita o PMM non init)
 *
 * @note L'indirizzo ritornato è sempre allineato a PAGE_SIZE
 * @note La pagina ritornata non è inizializzata (contenuto indefinito)
 */
void *pmm_alloc_page(void);

/**
 * @brief Alloca multiple pagine fisiche contigue
 *
 * Cerca un blocco contiguo di 'count' pagine libere e le alloca tutte.
 * Operazione atomica: o alloca tutte le pagine richieste o fallisce.
 *
 * @param count Numero di pagine contigue da allocare (deve essere > 0)
 * @return Indirizzo fisico della prima pagina del blocco
 * @return NULL se non disponibile un blocco contiguo della dimensione richiesta
 *
 * @note Utile per allocazioni che richiedono memoria fisicamente contigua
 * @note Più lenta di pmm_alloc_page() per allocazioni singole
 */
void *pmm_alloc_pages(size_t count);

/*
 * ============================================================================
 * MEMORY DEALLOCATION API
 * ============================================================================
 */

/**
 * @brief Libera una singola pagina fisica
 *
 * Marca la pagina come libera nel bitmap e aggiorna le statistiche.
 * Verifica che la pagina sia effettivamente allocata prima di liberarla.
 *
 * @param page Indirizzo fisico della pagina da liberare
 * @return PMM_SUCCESS se liberata correttamente
 * @return PMM_INVALID_ADDRESS se indirizzo non valido o non allineato
 * @return PMM_ALREADY_FREE se la pagina è già libera
 * @return PMM_NOT_INITIALIZED se PMM non inizializzato
 *
 * @note L'indirizzo deve essere esattamente quello ritornato da pmm_alloc_page
 * @note Dopo questa chiamata, l'accesso alla pagina è indefinito
 */
pmm_result_t pmm_free_page(void *page);

/**
 * @brief Libera multiple pagine fisiche contigue
 *
 * Libera un blocco di pagine contigue precedentemente allocato
 * con pmm_alloc_pages(). Operazione atomica.
 *
 * @param pages Indirizzo fisico della prima pagina del blocco
 * @param count Numero di pagine da liberare
 * @return PMM_SUCCESS se tutte le pagine sono state liberate
 * @return PMM_INVALID_ADDRESS se parametri non validi
 * @return PMM_ALREADY_FREE se una o più pagine sono già libere
 * @return PMM_NOT_INITIALIZED se PMM non inizializzato
 *
 * @note Tutti i parametri devono corrispondere esattamente alla chiamata
 *       pmm_alloc_pages() originale
 */
pmm_result_t pmm_free_pages(void *pages, size_t count);

/*
 * ============================================================================
 * QUERY AND INSPECTION API
 * ============================================================================
 */

/**
 * @brief Verifica se una pagina è libera
 *
 * Controlla lo stato di una specifica pagina senza modificarla.
 * Utile per debugging e verifiche.
 *
 * @param page Indirizzo fisico della pagina da verificare
 * @return true se la pagina è libera e disponibile per allocazione
 * @return false se occupata, non valida, o PMM non inizializzato
 */
bool pmm_is_page_free(void *page);

/**
 * @brief Ottiene le statistiche correnti del PMM
 *
 * Ritorna un puntatore alla struttura delle statistiche interne.
 * Le statistiche sono aggiornate in tempo reale ad ogni operazione.
 *
 * @return Puntatore costante alla struttura statistiche
 * @return NULL se PMM non inizializzato
 *
 * @note Il puntatore ritornato è valido fino al prossimo riavvio
 * @note Le statistiche sono read-only per il chiamante
 */
const pmm_stats_t *pmm_get_stats(void);

/**
 * @brief Ottiene informazioni dettagliate su una pagina specifica
 *
 * Permette di ottenere sia l'indice che lo stato di una pagina
 * in una singola chiamata efficiente.
 *
 * @param page Indirizzo della pagina da ispezionare
 * @param page_index[out] Puntatore dove salvare l'indice della pagina (opzionale)
 * @param is_free[out] Puntatore dove salvare se la pagina è libera (opzionale)
 * @return true se le informazioni sono valide
 * @return false se l'indirizzo è invalido o PMM non inizializzato
 *
 * @note I parametri di output possono essere NULL se non interessano
 */
bool pmm_get_page_info(void *page, u64 *page_index, bool *is_free);

/*
 * ============================================================================
 * ANALYSIS AND DEBUGGING API
 * ============================================================================
 */

/**
 * @brief Stampa informazioni dettagliate sullo stato del PMM
 *
 * Output comprensivo delle statistiche principali, uso memoria,
 * e informazioni diagnostiche. Utile per debugging e monitoring.
 */
void pmm_print_info(void);

/**
 * @brief Verifica l'integrità del bitmap interno
 *
 * Controlla che il bitmap sia consistente e che le statistiche
 * corrispondano allo stato effettivo. Operazione lenta ma completa.
 *
 * @return true se il bitmap è integro e consistente
 * @return false se rileva corruzioni o inconsistenze
 *
 * @note Operazione O(n) - da usare solo per debugging
 * @note Se ritorna false, il sistema potrebbe essere instabile
 */
bool pmm_check_integrity(void);

/**
 * @brief Valida rapidamente le statistiche interne senza scansione completa
 */
bool pmm_validate_stats(void);

/**
 * @brief Trova la sequenza più lunga di pagine libere contigue
 *
 * Analizza l'intero bitmap per trovare il blocco contiguo più grande
 * di pagine libere. Utile per analisi frammentazione e capacità massima.
 *
 * @param start_page[out] Puntatore dove salvare l'indice della prima pagina (opzionale)
 * @return Numero di pagine nella sequenza più lunga trovata
 * @return 0 se nessuna pagina libera o PMM non inizializzato
 *
 * @note Operazione O(n) - può essere lenta su sistemi con molta memoria
 * @note Aggiorna anche la statistica pmm_stats.largest_free_run
 */
size_t pmm_find_largest_free_run(size_t *start_page);

/**
 * @brief Stampa statistiche dettagliate di frammentazione
 *
 * Analizza la frammentazione della memoria e stampa informazioni
 * su blocchi contigui, frammentazione percentuale, e distribuzioni.
 */
void pmm_print_fragmentation_info(void);

/*
 * ============================================================================
 * ADVANCED ALLOCATION API (TODO - FUTURE EXTENSIONS)
 * ============================================================================
 */

/**
 * @brief Alloca pagine in una zona specifica di memoria
 *
 * Utile per allocazioni che devono essere in zone particolari
 * (es: sotto 16MB per DMA legacy, sopra 4GB per high memory).
 *
 * @param count Numero di pagine da allocare
 * @param min_addr Indirizzo fisico minimo (incluso)
 * @param max_addr Indirizzo fisico massimo (escluso)
 * @return Indirizzo fisico della prima pagina nel range specificato
 * @return NULL se nessun blocco disponibile nel range
 *
 * @note L'implementazione scorre il range dato cercando un blocco
 *       contiguo di pagine libere.
 */
void *pmm_alloc_pages_in_range(size_t count, u64 min_addr, u64 max_addr);

/**
 * @brief Alloca pagine con allineamento specifico
 *
 * Alloca un blocco di pagine con un allineamento specifico.
 * Utile per strutture che richiedono allineamenti particolari.
 *
 * @param pages Numero di pagine da allocare
 * @param alignment Allineamento richiesto in byte (deve essere potenza di 2)
 * @return Indirizzo fisico allineato al boundary specificato
 * @return NULL se non può soddisfare l'allineamento richiesto
 *
 * @note L'algoritmo percorre la memoria finché non trova un indirizzo
 *       che rispetti l'allineamento e abbia 'pages' pagine libere contigue.
 * @note alignment deve essere >= PAGE_SIZE e potenza di 2
 */
void *pmm_alloc_aligned(size_t pages, size_t alignment);

/*
 * ============================================================================
 * USAGE EXAMPLES
 * ============================================================================
 */

/*
// Basic single page allocation
void *page = pmm_alloc_page();
if (page) {
    // Use the page...
    pmm_free_page(page);
}

// Multi-page allocation for larger structures
void *buffer = pmm_alloc_pages(16); // 64KB contiguous
if (buffer) {
    // Use 16 contiguous pages...
    pmm_free_pages(buffer, 16);
}

// Check system memory status
const pmm_stats_t *stats = pmm_get_stats();
if (stats) {
    printf("Free memory: %lu MB\n", stats->free_pages * PAGE_SIZE / MB);
}

// Debugging and integrity checks
if (!pmm_check_integrity()) {
    panic("PMM corruption detected!");
}
*/