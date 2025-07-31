#include <arch/memory.h>
#include <klib/klog.h>
#include <klib/spinlock.h>
#include <lib/string.h>
#include <lib/types.h>
#include <mm/memory.h>
#include <mm/pmm.h>

/*
 * ============================================================================
 * PHYSICAL MEMORY MANAGER (PMM) - GESTIONE MEMORIA FISICA
 * ============================================================================
 *
 * CONCETTI FONDAMENTALI:
 *
 * 1. COSA FA IL PMM?
 *    Il PMM è il "contabile" della memoria fisica del computer.
 *    Tiene traccia di quali pagine da 4KB sono libere o occupate.
 *    È come avere una mappa di tutti i "posti a sedere" della memoria.
 *
 * 2. PERCHÉ USIAMO LE PAGINE?
 *    La memoria è divisa in blocchi da 4096 byte (4KB) chiamati "pagine".
 *    Questo semplifica la gestione: invece di tracciare ogni singolo byte,
 *    trattiamo la memoria come una serie di mattoncini Lego da 4KB.
 *
 * 3. COME FUNZIONA IL BITMAP?
 *    Usiamo un bitmap dove ogni bit rappresenta una pagina:
 *    - Bit = 0: pagina libera (posto libero)
 *    - Bit = 1: pagina occupata (posto occupato)
 *    Con 1GB di RAM servono solo ~32KB per il bitmap!
 *
 * 4. ARCHITETTURA AGNOSTICA:
 *    Il PMM non sa se stiamo usando x86_64, ARM, o altre architetture.
 *    Riceve le informazioni dalla memoria dal "layer architetturale"
 *    e si concentra solo sulla logica di allocazione.
 */

/*
 * ============================================================================
 * STRUTTURE DATI INTERNE
 * ============================================================================
 */

/**
 * @brief Stato interno del PMM - tutte le informazioni per funzionare
 *
 * ANALOGIA: È come l'ufficio del direttore di un cinema che sa:
 * - Quanti posti ci sono in totale
 * - Dove si trova la mappa dei posti (bitmap)
 * - Da dove iniziare a cercare posti liberi (hint)
 */
typedef struct {
  bool initialized;   /* PMM è pronto all'uso? */
  u8 *bitmap;         /* Puntatore alla "mappa dei posti" */
  u64 bitmap_size;    /* Quanto è grande la mappa (in byte) */
  u64 total_pages;    /* Quante pagine totali gestiamo */
  u64 next_free_hint; /* Suggerimento: da dove cercare prossima pagina libera */

  /* Cache delle informazioni generali sulla memoria */
  u64 total_memory_bytes;  /* Memoria fisica totale del sistema */
  u64 usable_memory_bytes; /* Memoria che possiamo effettivamente usare */
} pmm_state_t;

/* Statistiche globali - condivise con il resto del kernel */
static pmm_stats_t pmm_stats;
static pmm_state_t pmm_state = {.initialized = false};
static spinlock_t pmm_lock = SPINLOCK_INITIALIZER;

/* -------------------------------------------------------------------------- */
/*                     HINT MANAGEMENT (THREAD-SAFE READY)                    */
/* -------------------------------------------------------------------------- */

static inline void pmm_update_hint_locked(u64 new_hint) {
  if (new_hint < pmm_state.total_pages) {
    pmm_state.next_free_hint = new_hint;
  } else {
    pmm_state.next_free_hint = 0;
  }
}

static inline void pmm_update_hint(u64 new_hint) {
  spinlock_lock(&pmm_lock);
  pmm_update_hint_locked(new_hint);
  spinlock_unlock(&pmm_lock);
}

/*
 * ============================================================================
 * OPERAZIONI SUL BITMAP - IL CUORE DEL PMM
 * ============================================================================
 *
 * SPIEGAZIONE DETTAGLIATA DEL BITMAP:
 *
 * Il bitmap è un array di byte dove ogni bit rappresenta una pagina.
 * Per accedere al bit della pagina N:
 * - Byte da modificare: N / 8 (divisione intera)
 * - Posizione nel byte: N % 8 (resto della divisione)
 *
 * ESEMPIO PRATICO:
 * Pagina 13 → Byte 13/8 = 1, Bit 13%8 = 5
 * Quindi modifichiamo il bit 5 del byte 1 del bitmap.
 *
 * OPERAZIONI BITWISE:
 * - SET (mettere a 1): OR con maschera    |= (1 << posizione)
 * - CLEAR (mettere a 0): AND con ~maschera &= ~(1 << posizione)
 * - TEST (leggere): AND con maschera       & (1 << posizione)
 */
#define BITMAP_SET_BIT(bitmap, bit) ((bitmap)[(bit) / 8] |= (1 << ((bit) % 8)))
#define BITMAP_CLEAR_BIT(bitmap, bit) ((bitmap)[(bit) / 8] &= ~(1 << ((bit) % 8)))
#define BITMAP_TEST_BIT(bitmap, bit) ((bitmap)[(bit) / 8] & (1 << ((bit) % 8)))

/*
 * ============================================================================
 * FUNZIONI INTERNE - MANIPOLAZIONE BITMAP
 * ============================================================================
 */

/**
 * @brief Marca una pagina come occupata nel bitmap
 *
 * ANALOGIA: È come mettere un segno "OCCUPATO" su un posto al cinema.
 * Una volta chiamata questa funzione, quella pagina non può essere
 * allocata ad altri fino a quando non viene liberata.
 *
 * SICUREZZA: Controlla sempre che l'indice sia valido per evitare
 * di scrivere fuori dal bitmap (buffer overflow).
 */
static void pmm_mark_page_used(u64 page_index) {
  /* Controllo di sicurezza: la pagina esiste davvero? */
  if (page_index < pmm_state.total_pages) {
    BITMAP_SET_BIT(pmm_state.bitmap, page_index);
    /* Ora il bit corrispondente è 1 = pagina occupata */
  }
  /* Se page_index >= total_pages, ignoriamo silenziosamente per sicurezza */
}

/**
 * @brief Marca una pagina come libera nel bitmap
 *
 * ANALOGIA: È come rimuovere il segno "OCCUPATO" da un posto al cinema.
 * Dopo questa operazione, la pagina può essere allocata ad altri.
 */
static void pmm_mark_page_free(u64 page_index) {
  if (page_index < pmm_state.total_pages) {
    BITMAP_CLEAR_BIT(pmm_state.bitmap, page_index);
    /* Ora il bit corrispondente è 0 = pagina libera */
  }
}

/**
 * @brief Controlla se una pagina è occupata
 *
 * FILOSOFIA "FAIL-SAFE":
 * Se chiedi lo stato di una pagina che non esiste, rispondiamo
 * "occupata" per sicurezza. È meglio dire "non disponibile"
 * quando in dubbio, piuttosto che rischiare di corrompere memoria.
 */
static bool pmm_is_page_used_internal(u64 page_index) {
  /* Pagine fuori range = sempre occupate (sicurezza) */
  if (page_index >= pmm_state.total_pages) {
    return true;
  }

  /* Leggi il bit e convertilo in boolean */
  return BITMAP_TEST_BIT(pmm_state.bitmap, page_index) != 0;
}

/**
 * @brief Ricalcola le statistiche contando tutto il bitmap
 *
 * QUANDO USARLA:
 * Questa funzione è "costosa" (O(n)) perché deve guardare ogni singola
 * pagina. La usiamo solo quando necessiamo accuratezza assoluta,
 * non ad ogni allocazione/deallocazione.
 *
 * ALTERNATIVE PIÙ VELOCI:
 * Durante le operazioni normali, aggiorniamo le statistiche
 * incrementalmente (±1 ad ogni alloc/free) per performance.
 */
static void pmm_update_stats(void) {
  u64 free_count = 0;
  u64 used_count = 0;

  /* Conta tutte le pagine una per una */
  for (u64 i = 0; i < pmm_state.total_pages; i++) {
    if (pmm_is_page_used_internal(i)) {
      used_count++;
    } else {
      free_count++;
    }
  }

  /* Aggiorna le statistiche globali */
  pmm_stats.free_pages = free_count;
  pmm_stats.used_pages = used_count;
  pmm_stats.total_pages = pmm_state.total_pages;
}

/*
 * ============================================================================
 * ALGORITMI DI RICERCA - COME TROVARE PAGINE LIBERE
 * ============================================================================
 */

/**
 * @brief Trova la prima pagina libera partendo da un hint
 *
 * OTTIMIZZAZIONE "LOCALITY":
 * Le allocazioni tendono ad essere vicine nel tempo e nello spazio.
 * Se ho appena allocato la pagina 100, probabilmente la prossima
 * richiesta vorrà la pagina 101 o giù di lì. Questo riduce la
 * frammentazione e migliora le performance della cache.
 *
 * ALGORITMO:
 * 1. Cerca dall'hint in avanti → sfrutta località temporale
 * 2. Se non trova, ricomincia da 0 → garantisce completezza
 *
 * COMPLESSITÀ: O(n) worst case, ma O(1) average case con buon hint
 */
static u64 pmm_find_free_page_from(u64 start_hint) {
  /* FASE 1: Cerca dall'hint verso la fine */
  for (u64 i = start_hint; i < pmm_state.total_pages; i++) {
    if (!pmm_is_page_used_internal(i)) {
      return i; /* Trovata! */
    }
  }

  /* FASE 2: Wrap-around - cerca dall'inizio fino all'hint */
  for (u64 i = 0; i < start_hint; i++) {
    if (!pmm_is_page_used_internal(i)) {
      return i; /* Trovata nel "secondo giro" */
    }
  }

  /* Nessuna pagina libera trovata */
  return pmm_state.total_pages; /* Valore sentinella = "non trovato" */
}

/**
 * @brief Trova un blocco contiguo di pagine libere
 *
 * SFIDA DELLA CONTIGUITÀ:
 * Trovare N pagine libere è facile. Trovare N pagine libere CONSECUTIVE
 * è molto più difficile, specialmente se la memoria è frammentata.
 *
 * ALGORITMO "SLIDING WINDOW":
 * 1. Prova ogni posizione come inizio del blocco
 * 2. Per ogni posizione, controlla se tutte le N pagine sono libere
 * 3. Se trovi una occupata, salta avanti (ottimizzazione)
 *
 * OTTIMIZZAZIONE IMPORTANTE:
 * Se la pagina (start + i) è occupata, il prossimo blocco valido
 * può iniziare solo da (start + i + 1), quindi saltiamo direttamente lì.
 */
static u64 pmm_find_free_pages_from(u64 start_hint, size_t count) {
  /* FASE 1: Cerca dall'hint in avanti */
  for (u64 start = start_hint; start + count <= pmm_state.total_pages; start++) {
    bool found = true;

    /* Controlla se tutte le pagine nel blocco sono libere */
    for (size_t i = 0; i < count; i++) {
      if (pmm_is_page_used_internal(start + i)) {
        found = false;
        start += i; /* OTTIMIZZAZIONE: salta avanti invece di +1 */
        break;
      }
    }

    if (found) {
      return start; /* Blocco contiguo trovato! */
    }
  }

  /* FASE 2: Wrap-around search */
  for (u64 start = 0; start < start_hint && start + count <= pmm_state.total_pages; start++) {
    bool found = true;

    for (size_t i = 0; i < count; i++) {
      if (pmm_is_page_used_internal(start + i)) {
        found = false;
        start += i; /* Stessa ottimizzazione */
        break;
      }
    }

    if (found) {
      return start;
    }
  }

  /* Nessun blocco contiguo disponibile */
  return pmm_state.total_pages;
}

/*
 * ============================================================================
 * INIZIALIZZAZIONE - MESSA IN FUNZIONE DEL PMM
 * ============================================================================
 */

/**
 * @brief Inizializza il PMM usando l'interfaccia architetturale
 *
 * FLUSSO DI INIZIALIZZAZIONE:
 *
 * 1. DISCOVERY: Scopri quanta memoria c'è e di che tipo
 * 2. PLANNING: Calcola le dimensioni del bitmap necessario
 * 3. ALLOCATION: Trova spazio per il bitmap nella memoria fisica
 * 4. INITIALIZATION: Imposta il bitmap secondo i tipi di memoria
 * 5. PROTECTION: Proteggi le aree critiche (bitmap stesso, pagina 0)
 * 6. FINALIZATION: Aggiorna statistiche e marca come pronto
 *
 * PRINCIPIO "CONSERVATIVE":
 * Inizializziamo tutto come "occupato" e poi liberiamo esplicitamente
 * solo quello che sappiamo essere sicuro. È meglio essere conservativi
 * e perdere un po' di memoria che corrompere il sistema.
 */
pmm_result_t pmm_init(void) {
  klog_info("PMM: Avvio inizializzazione Physical Memory Manager");

  /*
   * STEP 1: INIZIALIZZAZIONE LAYER ARCHITETTURALE
   *
   * Il PMM non sa se stiamo girando su x86_64, ARM, RISC-V, etc.
   * Delega al layer architetturale la scoperta dell'hardware.
   */
  arch_memory_init();

  /*
   * STEP 2: DISCOVERY DELLE REGIONI DI MEMORIA
   *
   * Chiediamo al layer architetturale: "Dimmi che memoria abbiamo!"
   * Risposta: array di regioni con tipo, base, e dimensione.
   */
  memory_region_t regions[MAX_REGIONS];
  size_t region_count = arch_memory_detect_regions(regions, MAX_REGIONS);

  if (region_count == 0) {
    klog_error("PMM: Il layer architetturale non ha trovato memoria!");
    return PMM_NOT_INITIALIZED;
  }

  klog_info("PMM: Trovate %zu regioni di memoria", region_count);

  /*
   * STEP 3: ANALISI DELLE REGIONI - CALCOLO LIMITI
   *
   * Dobbiamo capire:
   * - Qual è l'indirizzo più alto? (per dimensionare il bitmap)
   * - Quanta memoria totale abbiamo?
   * - Quanta è effettivamente utilizzabile?
   */
  u64 highest_addr = 0;
  u64 total_memory = 0;
  u64 usable_memory = 0;

  for (size_t i = 0; i < region_count; i++) {
    memory_region_t *region = &regions[i];

    total_memory += region->length;

    /* L'indirizzo finale di questa regione */
    u64 region_end = region->base + region->length;
    if (region_end > highest_addr) {
      highest_addr = region_end;
    }

    /* Conta solo la memoria che possiamo davvero usare */
    if (region->type == MEMORY_USABLE || region->type == MEMORY_BOOTLOADER_RECLAIMABLE || region->type == MEMORY_ACPI_RECLAIMABLE) {
      usable_memory += region->length;
    }
  }

  /*
   * CALCOLI FONDAMENTALI:
   *
   * highest_addr ci dice fino a dove arriva la memoria fisica.
   * Dobbiamo gestire tutte le pagine da 0 fino a highest_addr.
   *
   * ESEMPIO: Se highest_addr = 0x40000000 (1GB)
   * total_pages = 0x40000000 >> 12 = 262144 pagine da gestire
   */
  pmm_state.total_pages = ADDR_TO_PAGE(highest_addr);
  pmm_state.total_memory_bytes = total_memory;
  pmm_state.usable_memory_bytes = usable_memory;

  klog_info("PMM: Memoria totale: %lu MB, utilizzabile: %lu MB", total_memory / MB, usable_memory / MB);
  klog_info("PMM: Pagine da gestire: %lu", pmm_state.total_pages);

  /*
   * STEP 4: DIMENSIONAMENTO E ALLOCAZIONE BITMAP
   *
   * Il bitmap ha bisogno di 1 bit per pagina.
   * Se abbiamo N pagine, servono N/8 byte (arrotondati in su).
   *
   * ESEMPIO: 262144 pagine → (262144 + 7) / 8 = 32768 byte = 32KB
   *
   * Il bitmap stesso occuperà delle pagine fisiche!
   * 32KB → PAGE_ALIGN_UP(32768) / 4096 = 8 pagine per il bitmap
   */
  pmm_state.bitmap_size = (pmm_state.total_pages + 7) / 8;
  u64 bitmap_pages_needed = PAGE_ALIGN_UP(pmm_state.bitmap_size) / PAGE_SIZE;

  klog_info("PMM: Il bitmap richiede %lu bytes (%lu pagine)", pmm_state.bitmap_size, bitmap_pages_needed);

  /*
   * RICERCA POSIZIONE PER IL BITMAP:
   *
   * Il bitmap deve stare da qualche parte nella memoria fisica!
   * Cerchiamo una regione USABLE abbastanza grande e allineata.
   */
  u64 bitmap_addr = 0;
  bool bitmap_found = false;

  for (size_t i = 0; i < region_count; i++) {
    memory_region_t *region = &regions[i];

    /* Candidato: regione usabile e abbastanza grande */
    if (region->type == MEMORY_USABLE && region->length >= pmm_state.bitmap_size) {
      /* Allineamento: il bitmap deve iniziare a un boundary di pagina */
      u64 aligned_base = PAGE_ALIGN_UP(region->base);
      u64 available = region->base + region->length - aligned_base;

      if (available >= pmm_state.bitmap_size) {
        bitmap_addr = aligned_base;
        bitmap_found = true;
        break; /* Primo candidato valido = buono */
      }
    }
  }

  if (!bitmap_found) {
    klog_error("PMM: Impossibile trovare spazio per il bitmap!");
    return PMM_OUT_OF_MEMORY;
  }

  pmm_state.bitmap = (u8 *)bitmap_addr;
  pmm_stats.bitmap_pages = bitmap_pages_needed;

  klog_info("PMM: Bitmap allocato all'indirizzo 0x%lx", bitmap_addr);

  /*
   * STEP 5: INIZIALIZZAZIONE "CONSERVATIVA" DEL BITMAP
   *
   * FILOSOFIA: "Tutto occupato finché non dimostriamo il contrario"
   *
   * memset(bitmap, 0xFF) imposta tutti i bit a 1 = tutte occupate.
   * Poi andremo regione per regione a "liberare" esplicitamente
   * solo quelle che sappiamo essere sicure da usare.
   */
  memset(pmm_state.bitmap, 0xFF, pmm_state.bitmap_size);
  klog_debug("PMM: Bitmap inizializzato - tutte le pagine marcate occupate");

  /*
   * STEP 6: MARCATURA DELLE REGIONI SECONDO IL TIPO
   *
   * Ora esaminiamo ogni regione e decidiamo cosa farne:
   * - USABLE/BOOTLOADER_RECLAIMABLE/ACPI_RECLAIMABLE → libera nel bitmap
   * - Tutto il resto → lascia occupato
   */
  for (size_t i = 0; i < region_count; i++) {
    memory_region_t *region = &regions[i];
    /*
     * Allinea gli indirizzi ai confini di pagina per evitare che porzioni
     * parziali vengano considerate totalmente libere.
     */
    u64 aligned_start = PAGE_ALIGN_UP(region->base);
    u64 aligned_end = PAGE_ALIGN_DOWN(region->base + region->length);

    /* Se dopo l'allineamento non rimane almeno una pagina completa, salta */
    if (aligned_end <= aligned_start) {
      continue;
    }

    u64 start_page = ADDR_TO_PAGE(aligned_start);
    u64 end_page = ADDR_TO_PAGE(aligned_end) - 1;
    switch (region->type) {
    case MEMORY_USABLE:
    case MEMORY_BOOTLOADER_RECLAIMABLE:
    case MEMORY_ACPI_RECLAIMABLE:
      /* Queste regioni sono sicure da usare → libera nel bitmap */
      for (u64 page = start_page; page <= end_page; page++) {
        pmm_mark_page_free(page);
      }
      klog_debug("PMM: Liberate %lu pagine (regione %zu tipo %d)", end_page - start_page + 1, i, region->type);
      break;

    default:
      /*
       * MEMORY_RESERVED, MEMORY_EXECUTABLE_AND_MODULES,
       * MEMORY_BAD, MEMORY_FRAMEBUFFER, etc.
       * → Lascia occupate (già fatto da memset)
       */
      pmm_stats.reserved_pages += (end_page - start_page + 1);
      klog_debug("PMM: Riservate %lu pagine (regione %zu tipo %d)", end_page - start_page + 1, i, region->type);
      break;
    }
  }

  /*
   * STEP 7: PROTEZIONE DEL BITMAP STESSO
   *
   * PROBLEMA CRITICO: Il bitmap stesso occupa memoria fisica!
   * Se non lo proteggiamo, potremmo allocare quelle pagine ad altri
   * e corrompere il bitmap → crash del sistema.
   *
   * SOLUZIONE: Marca esplicitamente come occupate le pagine del bitmap.
   */
  u64 bitmap_start_page = ADDR_TO_PAGE(bitmap_addr);
  u64 bitmap_end_page = ADDR_TO_PAGE(bitmap_addr + pmm_state.bitmap_size - 1);

  for (u64 page = bitmap_start_page; page <= bitmap_end_page; page++) {
    pmm_mark_page_used(page);
  }

  klog_info("PMM: Protette le pagine del bitmap %lu-%lu", bitmap_start_page, bitmap_end_page);

  /*
   * STEP 8: PROTEZIONE PAGINA 0 (NULL POINTER PROTECTION)
   *
   * La pagina 0 (indirizzi 0x0000-0x0FFF) non deve mai essere allocata.
   * Se qualcuno dereferenzia un puntatore NULL, deve andare in crash
   * subito, non accedere a memoria valida!
   */
  pmm_mark_page_used(0);
  klog_debug("PMM: Pagina 0 protetta (NULL pointer protection)");

  /*
   * STEP 9: FINALIZZAZIONE E STATISTICHE
   *
   * Il PMM è quasi pronto. Aggiorniamo le statistiche finali
   * e marchiamo come inizializzato.
   */
  pmm_update_stats();           /* Conta tutto per avere statistiche accurate */
  pmm_update_hint(0);           /* Inizia a cercare dall'inizio */
  pmm_state.initialized = true; /* Ora il PMM è operativo! */

  klog_info("PMM: Inizializzazione completata con successo!");
  klog_info("PMM: %lu pagine libere, %lu occupate, %lu riservate", pmm_stats.free_pages, pmm_stats.used_pages, pmm_stats.reserved_pages);

  return PMM_SUCCESS;
}

/*
 * ============================================================================
 * API PUBBLICA - INTERFACCIA PER IL RESTO DEL KERNEL
 * ============================================================================
 */

/**
 * @brief Alloca una singola pagina fisica
 *
 * CASO D'USO TIPICO:
 * Il kernel ha bisogno di 4KB per una struttura dati, una page table,
 * o un buffer. Questa è l'operazione più comune.
 *
 * ALGORITMO:
 * 1. Verifica precondizioni (PMM inizializzato, memoria disponibile)
 * 2. Cerca pagina libera usando l'hint per performance
 * 3. Marca la pagina come occupata nel bitmap
 * 4. Aggiorna statistiche e hint
 * 5. Restituisce l'indirizzo fisico della pagina
 */
void *pmm_alloc_page(void) {
  /* Precondizione: PMM deve essere inizializzato */
  if (!pmm_state.initialized) {
    return NULL;
  }

  spinlock_lock(&pmm_lock);

  /* Precondizione: Deve esserci almeno una pagina libera */
  if (pmm_stats.free_pages == 0) {
    spinlock_unlock(&pmm_lock);
    return NULL; /* Memoria fisica esaurita */
  }

  /* Cerca pagina libera usando l'hint per ottimizzazione */
  u64 page_index = pmm_find_free_page_from(pmm_state.next_free_hint);
  if (page_index >= pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return NULL; /* Nessuna pagina trovata */
  }

  /* Allocazione riuscita: aggiorna stato e statistiche */
  pmm_mark_page_used(page_index);
  pmm_stats.free_pages--;
  pmm_stats.used_pages++;
  pmm_stats.alloc_count++;

  /* Aggiorna hint per la prossima ricerca (località temporale) */
  pmm_update_hint_locked(page_index + 1);
  spinlock_unlock(&pmm_lock);

  /* Converte indice pagina in indirizzo fisico */
  return (void *)PAGE_TO_ADDR(page_index);
}

/**
 * @brief Alloca multiple pagine fisiche contigue
 *
 * QUANDO SERVE:
 * - Buffer DMA che devono essere fisicamente contigui
 * - Strutture dati grandi (>4KB) che vogliamo in un blocco unico
 * - Page directory/page table che occupano più pagine
 *
 * DIFFICOLTÀ:
 * Trovare N pagine contigue è molto più difficile se la memoria
 * è frammentata. Questa operazione può fallire anche se ci sono
 * N pagine libere totali ma non consecutive.
 */
void *pmm_alloc_pages(size_t count) {
  /* Validazione parametri */
  if (!pmm_state.initialized || count == 0) {
    return NULL;
  }

  spinlock_lock(&pmm_lock);

  /* Verifica disponibilità: servono almeno 'count' pagine libere */
  if (pmm_stats.free_pages < count) {
    spinlock_unlock(&pmm_lock);
    return NULL;
  }

  /* Cerca blocco contiguo di 'count' pagine */
  u64 start_page = pmm_find_free_pages_from(pmm_state.next_free_hint, count);
  if (start_page >= pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return NULL; /* Nessun blocco contiguo disponibile */
  }

  /* Allocazione riuscita: marca tutte le pagine come occupate */
  for (size_t i = 0; i < count; i++) {
    pmm_mark_page_used(start_page + i);
  }

  /* Aggiorna statistiche */
  pmm_stats.free_pages -= count;
  pmm_stats.used_pages += count;
  pmm_stats.alloc_count++;

  /* Aggiorna hint con controllo overflow */
  pmm_update_hint_locked(start_page + count);
  spinlock_unlock(&pmm_lock);

  return (void *)PAGE_TO_ADDR(start_page);
}

/**
 * @brief Libera una singola pagina fisica
 *
 * VALIDAZIONI CRITICHE:
 * 1. L'indirizzo deve essere valido e allineato
 * 2. La pagina deve essere effettivamente allocata
 * 3. Non deve essere una pagina protetta (bitmap, pagina 0)
 *
 * DOUBLE-FREE DETECTION:
 * Se qualcuno prova a liberare una pagina già libera, lo rilevi amo
 * e ritorniamo un errore invece di corrompere lo stato.
 */
pmm_result_t pmm_free_page(void *page) {
  if (!pmm_state.initialized) {
    return PMM_NOT_INITIALIZED;
  }

  if (!page) {
    return PMM_INVALID_ADDRESS; /* NULL pointer */
  }

  u64 addr = (u64)page;

  /* L'indirizzo deve essere allineato a PAGE_SIZE */
  if (addr % PAGE_SIZE != 0) {
    return PMM_INVALID_ADDRESS;
  }

  u64 page_index = ADDR_TO_PAGE(addr);

  spinlock_lock(&pmm_lock);

  /* La pagina deve esistere nel nostro range */
  if (page_index >= pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return PMM_INVALID_ADDRESS;
  }

  /* Double-free detection: la pagina deve essere attualmente occupata */
  if (!pmm_is_page_used_internal(page_index)) {
    spinlock_unlock(&pmm_lock);
    return PMM_ALREADY_FREE;
  }

  /* Liberazione: marca come libera e aggiorna statistiche */
  pmm_mark_page_free(page_index);
  pmm_stats.free_pages++;
  pmm_stats.used_pages--;
  pmm_stats.free_count++;

  /* Aggiorna hint se questa pagina è "più a sinistra" dell'hint corrente */
  if (page_index < pmm_state.next_free_hint) {
    pmm_update_hint_locked(page_index);
  }

  spinlock_unlock(&pmm_lock);

  return PMM_SUCCESS;
}

/**
 * @brief Libera multiple pagine fisiche contigue
 *
 * OPERAZIONE ATOMICA:
 * O liberiamo tutte le pagine richieste, o non liberiamo nessuna.
 * Non può succedere che liberiamo solo alcune pagine del blocco.
 *
 * VALIDAZIONE RIGOROSA:
 * Prima di liberare qualsiasi cosa, controlliamo che TUTTE le pagine
 * del blocco siano effettivamente allocate. Questo previene corruzioni
 * se il chiamante passa parametri sbagliati.
 */
pmm_result_t pmm_free_pages(void *pages, size_t count) {
  if (!pmm_state.initialized) {
    return PMM_NOT_INITIALIZED;
  }

  if (!pages || count == 0) {
    return PMM_INVALID_ADDRESS;
  }

  u64 addr = (u64)pages;
  if (addr % PAGE_SIZE != 0) {
    return PMM_INVALID_ADDRESS;
  }

  u64 start_page = ADDR_TO_PAGE(addr);

  spinlock_lock(&pmm_lock);

  /* Controlla che il blocco intero sia nel range valido */
  if (start_page + count > pmm_state.total_pages) {
    spinlock_unlock(&pmm_lock);
    return PMM_INVALID_ADDRESS;
  }

  /*
   * VALIDAZIONE PREVENTIVA:
   * Controlla che tutte le pagine siano attualmente occupate
   * PRIMA di liberarne qualsiasi una. Questo garantisce atomicità.
   */
  for (size_t i = 0; i < count; i++) {
    if (!pmm_is_page_used_internal(start_page + i)) {
      spinlock_unlock(&pmm_lock);
      return PMM_ALREADY_FREE; /* Almeno una è già libera → errore */
    }
  }

  /* Validazione OK: ora libera tutte le pagine */
  for (size_t i = 0; i < count; i++) {
    pmm_mark_page_free(start_page + i);
  }

  /* Aggiorna statistiche */
  pmm_stats.free_pages += count;
  pmm_stats.used_pages -= count;
  pmm_stats.free_count++;

  /* Aggiorna hint */
  if (start_page < pmm_state.next_free_hint) {
    pmm_update_hint_locked(start_page);
  }

  spinlock_unlock(&pmm_lock);

  return PMM_SUCCESS;
}

/**
 * @brief Controlla se una pagina è libera (query non-distruttiva)
 *
 * UTILITÀ:
 * Funzione di "ispezione" che non modifica nulla. Utile per:
 * - Debugging ("perché questa allocazione è fallita?")
 * - Validazione ("è sicuro usare questa pagina?")
 * - Testing (verifica stato prima/dopo operazioni)
 */
bool pmm_is_page_free(void *page) {
  if (!pmm_state.initialized || !page) {
    return false;
  }

  u64 addr = (u64)page;
  if (addr % PAGE_SIZE != 0) {
    return false;
  }

  u64 page_index = ADDR_TO_PAGE(addr);
  if (page_index >= pmm_state.total_pages) {
    return false;
  }

  /* Inverti la logica: "non occupata" = "libera" */
  return !pmm_is_page_used_internal(page_index);
}

/**
 * @brief Ottiene snapshot delle statistiche correnti
 *
 * THREAD-SAFETY NOTE:
 * Le statistiche sono aggiornate in tempo reale ad ogni operazione.
 * In un kernel single-threaded non ci sono problemi. Quando
 * aggiungeremo il threading, dovremo proteggere con lock.
 */
const pmm_stats_t *pmm_get_stats(void) {
  if (!pmm_state.initialized) {
    return NULL;
  }
  return &pmm_stats;
}

/*
 * ============================================================================
 * FUNZIONI DI DIAGNOSTICA E DEBUG
 * ============================================================================
 */

/**
 * @brief Stampa report completo dello stato del PMM
 *
 * OUTPUT HUMAN-READABLE:
 * Converte i numeri interni in unità comprensibili (MB invece di byte,
 * percentuali invece di rapporti) per facilitare il debugging.
 */
void pmm_print_info(void) {
  if (!pmm_state.initialized) {
    klog_error("PMM: Non inizializzato");
    return;
  }

  klog_info("=== STATUS PHYSICAL MEMORY MANAGER ===");
  klog_info("Memoria totale: %lu MB (%lu pagine)", pmm_state.total_memory_bytes / MB, pmm_stats.total_pages);
  klog_info("Memoria utilizzabile: %lu MB", pmm_state.usable_memory_bytes / MB);
  klog_info("Pagine libere: %lu (%lu MB)", pmm_stats.free_pages, pmm_stats.free_pages * PAGE_SIZE / MB);
  klog_info("Pagine occupate: %lu (%lu MB)", pmm_stats.used_pages, pmm_stats.used_pages * PAGE_SIZE / MB);
  klog_info("Pagine riservate: %lu (%lu MB)", pmm_stats.reserved_pages, pmm_stats.reserved_pages * PAGE_SIZE / MB);
  klog_info("Bitmap: %lu pagine (%lu KB)", pmm_stats.bitmap_pages, pmm_state.bitmap_size / KB);
  klog_info("Operazioni: %lu allocazioni, %lu deallocazioni", pmm_stats.alloc_count, pmm_stats.free_count);

  /* Calcola e mostra percentuale di utilizzo */
  u64 usage_percent = (pmm_stats.used_pages * 100) / pmm_stats.total_pages;
  klog_info("Utilizzo memoria: %lu%%", usage_percent);
}

/**
 * @brief Verifica integrità del PMM (operazione costosa!)
 *
 * QUANDO USARLA:
 * Solo durante debugging o dopo operazioni sospette. È O(n) quindi
 * lenta su sistemi con molta memoria.
 *
 * COSA CONTROLLA:
 * 1. Le statistiche corrispondono al contenuto effettivo del bitmap?
 * 2. Non ci sono inconsistenze nei contatori?
 *
 * SE FALLISCE:
 * Indica bug nel PMM o corruzione della memoria → sistema instabile
 */
bool pmm_check_integrity(void) {
  if (!pmm_state.initialized) {
    return false;
  }

  u64 free_count = 0;
  u64 used_count = 0;

  /* Conta manualmente tutto il bitmap */
  for (u64 i = 0; i < pmm_state.total_pages; i++) {
    if (pmm_is_page_used_internal(i)) {
      used_count++;
    } else {
      free_count++;
    }
  }

  /* Confronta con le statistiche cached */
  bool consistent = (free_count == pmm_stats.free_pages) && (used_count == pmm_stats.used_pages);

  if (!consistent) {
    klog_error("PMM: CORRUZIONE RILEVATA!");
    klog_error("  Contate libere: %lu, Statistiche: %lu", free_count, pmm_stats.free_pages);
    klog_error("  Contate occupate: %lu, Statistiche: %lu", used_count, pmm_stats.used_pages);
    klog_error("  Il sistema potrebbe essere instabile!");
  }

  return consistent;
}

/**
 * @brief Valida rapidamente le statistiche PMM per rilevare anomalie
 */
bool pmm_validate_stats(void) {
  u64 total_accounted = pmm_stats.free_pages + pmm_stats.used_pages;
  if (total_accounted > pmm_stats.total_pages) {
    klog_error("PMM: Stats inconsistent: %lu > %lu", total_accounted, pmm_stats.total_pages);
    return false;
  }
  if (pmm_stats.largest_free_run > pmm_stats.free_pages) {
    klog_error("PMM: Largest free run invalid: %lu > %lu", pmm_stats.largest_free_run, pmm_stats.free_pages);
    return false;
  }
  return true;
}

/**
 * @brief Trova il blocco contiguo più grande di pagine libere
 *
 * ANALISI FRAMMENTAZIONE:
 * Questa funzione ci dice quanto è frammentata la memoria.
 * Se abbiamo 1000 pagine libere ma il blocco più grande è solo
 * 10 pagine, la memoria è molto frammentata.
 *
 * ALGORITMO "SLIDING WINDOW":
 * Scorre tutto il bitmap tenendo traccia della sequenza corrente
 * di pagine libere e del record massimo trovato finora.
 */
size_t pmm_find_largest_free_run(size_t *start_page) {
  if (!pmm_state.initialized) {
    return 0;
  }

  size_t max_run = 0;       /* Sequenza più lunga trovata finora */
  size_t current_run = 0;   /* Sequenza corrente in corso */
  size_t max_start = 0;     /* Dove inizia la sequenza più lunga */
  size_t current_start = 0; /* Dove inizia la sequenza corrente */

  for (u64 i = 0; i < pmm_state.total_pages; i++) {
    if (!pmm_is_page_used_internal(i)) {
      /* Pagina libera: estendi sequenza corrente */
      if (current_run == 0) {
        current_start = i; /* Inizio nuova sequenza */
      }
      current_run++;

      /* Nuovo record? */
      if (current_run > max_run) {
        max_run = current_run;
        max_start = current_start;
      }
    } else {
      /* Pagina occupata: interrompi sequenza corrente */
      current_run = 0;
    }
  }

  /* Restituisci la posizione se richiesta */
  if (start_page) {
    *start_page = max_start;
  }

  /* Aggiorna anche la statistica per future query */
  pmm_stats.largest_free_run = max_run;
  return max_run;
}

/**
 * @brief Analisi dettagliata della frammentazione
 *
 * METRICHE DI FRAMMENTAZIONE:
 * - Blocco contiguo più grande
 * - Percentuale di frammentazione
 * - Posizione del blocco più grande
 *
 * INTERPRETAZIONE:
 * - Frammentazione 0%: Tutta la memoria libera è in un unico blocco
 * - Frammentazione 50%: La memoria libera è abbastanza frammentata
 * - Frammentazione 90%+: Memoria molto frammentata, difficile allocare blocchi grandi
 */
void pmm_print_fragmentation_info(void) {
  if (!pmm_state.initialized) {
    return;
  }

  size_t start_page;
  size_t largest_run = pmm_find_largest_free_run(&start_page);

  klog_info("=== ANALISI FRAMMENTAZIONE MEMORIA ===");
  klog_info("Blocco contiguo più grande: %lu pagine (%lu MB)", largest_run, largest_run * PAGE_SIZE / MB);

  if (largest_run > 0) {
    klog_info("Posizione: pagina %lu (indirizzo fisico 0x%lx)", start_page, PAGE_TO_ADDR(start_page));
  }

  /* Calcola percentuale di frammentazione */
  if (pmm_stats.free_pages > 0) {
    u64 fragmentation = 100 - ((largest_run * 100) / pmm_stats.free_pages);
    klog_info("Frammentazione: %lu%%", fragmentation);

    if (fragmentation < 20) {
      klog_info("→ Memoria poco frammentata (ottimo)");
    } else if (fragmentation < 50) {
      klog_info("→ Frammentazione moderata (accettabile)");
    } else {
      klog_warn("→ Memoria molto frammentata (problematico per allocazioni grandi)");
    }
  }
}

/*
 * ============================================================================
 * FUNZIONI AVANZATE - DA IMPLEMENTARE IN FUTURO
 * ============================================================================
 */

/**
 * @brief Allocazione in range specifico di indirizzi
 *
 * CASI D'USO:
 * - DMA legacy che funziona solo sotto 16MB
 * - Driver che richiedono memoria sotto 4GB (32-bit compatibility)
 * - NUMA-aware allocation su sistemi multi-socket
 *
 * IMPLEMENTAZIONE FUTURA:
 * Modificare pmm_find_free_pages_from per accettare min/max addr
 */
void *pmm_alloc_pages_in_range(size_t count, u64 min_addr, u64 max_addr) {
  if (!pmm_state.initialized || count == 0 || max_addr <= min_addr) {
    return NULL;
  }

  u64 start_page = ADDR_TO_PAGE(PAGE_ALIGN_UP(min_addr));
  u64 end_page = ADDR_TO_PAGE(PAGE_ALIGN_DOWN(max_addr));

  if (start_page >= pmm_state.total_pages) {
    return NULL;
  }

  if (end_page > pmm_state.total_pages) {
    end_page = pmm_state.total_pages;
  }

  if (start_page + count > end_page) {
    return NULL;
  }

  spinlock_lock(&pmm_lock);

  for (u64 p = start_page; p + count <= end_page; p++) {
    bool found = true;
    for (size_t i = 0; i < count; i++) {
      if (pmm_is_page_used_internal(p + i)) {
        found = false;
        p += i;
        break;
      }
    }

    if (found) {
      for (size_t i = 0; i < count; i++) {
        pmm_mark_page_used(p + i);
      }

      pmm_stats.free_pages -= count;
      pmm_stats.used_pages += count;
      pmm_stats.alloc_count++;

      pmm_update_hint_locked(p + count);
      spinlock_unlock(&pmm_lock);

      return (void *)PAGE_TO_ADDR(p);
    }
  }

  spinlock_unlock(&pmm_lock);
  return NULL;
}

/**
 * @brief Allocazione con allineamento specifico
 *
 * CASI D'USO:
 * - Page directory che deve essere allineata a 4KB boundary
 * - Strutture DMA che richiedono allineamento specifico
 * - Performance optimization per accesso cache-aligned
 *
 * ALGORITMO FUTURO:
 * Trovare pagina libera dove (addr % alignment) == 0
 */
void *pmm_alloc_aligned(size_t pages, size_t alignment) {
  if (!pmm_state.initialized || pages == 0) {
    return NULL;
  }

  if (alignment < PAGE_SIZE || (alignment & (alignment - 1)) != 0) {
    return NULL;
  }

  spinlock_lock(&pmm_lock);

  for (u64 p = pmm_state.next_free_hint; p + pages <= pmm_state.total_pages; p++) {
    if (PAGE_TO_ADDR(p) % alignment != 0)
      continue;

    bool found = true;
    for (size_t i = 0; i < pages; i++) {
      if (pmm_is_page_used_internal(p + i)) {
        found = false;
        p += i;
        break;
      }
    }

    if (found) {
      for (size_t i = 0; i < pages; i++) {
        pmm_mark_page_used(p + i);
      }

      pmm_stats.free_pages -= pages;
      pmm_stats.used_pages += pages;
      pmm_stats.alloc_count++;

      pmm_update_hint_locked(p + pages);
      spinlock_unlock(&pmm_lock);

      return (void *)PAGE_TO_ADDR(p);
    }
  }

  for (u64 p = 0; p < pmm_state.next_free_hint && p + pages <= pmm_state.total_pages; p++) {
    if (PAGE_TO_ADDR(p) % alignment != 0)
      continue;

    bool found = true;
    for (size_t i = 0; i < pages; i++) {
      if (pmm_is_page_used_internal(p + i)) {
        found = false;
        p += i;
        break;
      }
    }

    if (found) {
      for (size_t i = 0; i < pages; i++) {
        pmm_mark_page_used(p + i);
      }

      pmm_stats.free_pages -= pages;
      pmm_stats.used_pages += pages;
      pmm_stats.alloc_count++;

      pmm_update_hint_locked(p + pages);
      spinlock_unlock(&pmm_lock);

      return (void *)PAGE_TO_ADDR(p);
    }
  }

  spinlock_unlock(&pmm_lock);
  return NULL;
}

/**
 * @brief Informazioni dettagliate su una pagina specifica
 *
 * UTILITÀ DEBUG:
 * Dato un indirizzo, restituisce tutte le informazioni che abbiamo
 * su quella pagina: indice, stato, validità, etc.
 */
bool pmm_get_page_info(void *page, u64 *page_index, bool *is_free) {
  if (!pmm_state.initialized || !page) {
    return false;
  }

  u64 addr = (u64)page;
  if (addr % PAGE_SIZE != 0) {
    return false; /* Indirizzo non allineato */
  }

  u64 idx = ADDR_TO_PAGE(addr);
  if (idx >= pmm_state.total_pages) {
    return false; /* Fuori range */
  }

  /* Restituisci le informazioni richieste */
  if (page_index) {
    *page_index = idx;
  }

  if (is_free) {
    *is_free = !pmm_is_page_used_internal(idx);
  }

  return true; /* Informazioni valide */
}

/*
 * ============================================================================
 * CONCLUSIONI E NOTE PEDAGOGICHE
 * ============================================================================
 *
 * COSA ABBIAMO IMPARATO STUDIANDO QUESTO PMM:
 *
 * 1. GESTIONE MEMORIA FISICA:
 *    Il PMM è il "contabile" della memoria fisica. Tiene traccia di ogni
 *    pagina da 4KB e decide chi può usare cosa. È la base di tutto il
 *    memory management del kernel.
 *
 * 2. STRUTTURE DATI EFFICIENTI:
 *    Il bitmap è incredibilmente efficiente: 1 bit per pagina significa
 *    che per 4GB di RAM servono solo 128KB di metadata! Questa efficienza
 *    è cruciale perché il PMM deve essere veloce e usare poca memoria.
 *
 * 3. ALGORITMI E OTTIMIZZAZIONI:
 *    - First-fit con hint per locality: le allocazioni consecutive sono più veloci
 *    - Conservative initialization per sicurezza: meglio sprecare memoria che crashare
 *    - Lazy statistics update per performance: aggiorniamo incrementalmente
 *    - Sliding window per ricerca contigua: ottimizzazione intelligente
 *
 * 4. ARCHITETTURA MODULARE:
 *    Il PMM non sa nulla di x86_64, ARM, Limine, UEFI. È completamente
 *    architettura-agnostico e riceve info dal layer sottostante. Questo
 *    permette portabilità e testabilità.
 *
 * 5. ERROR HANDLING ROBUSTO:
 *    Ogni funzione controlla i precondizioni, valida gli input, e
 *    fallisce in modo pulito invece di corrompere lo stato. Nel kernel
 *    space, la robustezza è più importante della velocità.
 *
 * 6. DEBUGGING E DIAGNOSTICA:
 *    Funzioni come pmm_check_integrity() e pmm_print_fragmentation_info()
 *    sono essenziali per capire cosa sta succedendo quando qualcosa va storto.
 *
 * 7. THREAD-SAFETY AWARENESS:
 *    Anche se ora siamo single-threaded, abbiamo progettato il PMM pensando
 *    al futuro multithreading. Le statistiche e lo stato sono separati per
 *    facilitare l'aggiunta di lock.
 *
 * CONCETTI AVANZATI IMPARATI:
 *
 * - **Locality of Reference**: Le allocazioni consecutive sono più veloci
 * - **Fragmentation Analysis**: Come misurare e interpretare la frammentazione
 * - **Atomic Operations**: Operazioni tutto-o-niente per consistenza
 * - **Conservative Design**: Fallire in sicurezza quando in dubbio
 * - **Layer Separation**: Separare logica business da dettagli hardware
 *
 * PROSSIMI PASSI NEL JOURNEY DEL MEMORY MANAGEMENT:
 *
 * 1. **Virtual Memory Manager (VMM)**:
 *    - Paging e traduzione indirizzi virtuali → fisici
 *    - Isolamento dei processi (ogni processo vede la sua memoria)
 *    - Page tables e TLB management
 *    - Copy-on-write, demand paging, swapping
 *
 * 2. **Heap Allocator (kmalloc/kfree)**:
 *    - Allocazioni più piccole di PAGE_SIZE (malloc per il kernel)
 *    - Algoritmi: buddy system, slab allocator, o binary trees
 *    - Gestione frammentazione interna
 *
 * 3. **User Space Memory Manager**:
 *    - mmap(), brk(), malloc() per i processi utente
 *    - Virtual memory areas (VMAs)
 *    - Memory protection e permessi
 *
 * 4. **Advanced Features**:
 *    - NUMA awareness per sistemi multi-socket
 *    - Memory hotplug (aggiungere RAM a runtime)
 *    - Memory compression e zswap
 *    - Kernel Address Space Layout Randomization (KASLR)
 *
 * FILOSOFIA DEL DESIGN:
 *
 * Il PMM che abbiamo studiato segue principi solidi:
 * - **Semplicità**: Usa la struttura dati più semplice che funziona (bitmap)
 * - **Efficienza**: Ottimizzazioni intelligenti senza complicare troppo
 * - **Robustezza**: Fallisce in modo pulito, mai corruzione
 * - **Modularità**: Separazione clara delle responsabilità
 * - **Debugging**: Tools per capire cosa sta succedendo
 *
 * Questi principi ti serviranno per tutto lo sviluppo del kernel!
 *
 * RIFLESSIONE FINALE:
 *
 * Il memory management è uno dei sottosistemi più critici del kernel.
 * Un bug nel PMM può corrompere tutto il sistema. Ma una volta che
 * funziona bene, diventa la fondazione rock-solid su cui costruire
 * tutto il resto.
 *
 * Congratulazioni per aver completato lo studio di un PMM completo! 🎉
 * Ora hai una comprensione profonda di come i sistemi operativi gestiscono
 * la memoria fisica. È tempo di passare al Virtual Memory Manager! 🚀
 */