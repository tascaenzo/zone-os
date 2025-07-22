#include <arch/x86_64/paging.h>
#include <bootloader/limine.h>
#include <klib/klog.h>
#include <lib/string.h>
#include <lib/types.h>
#include <mm/memory.h>
#include <mm/pmm.h>

/*
 * Physical Memory Manager (PMM) - Gestione della memoria fisica
 *
 * Il PMM mantiene uno stato interno che include:
 * - Un bitmap per tracciare le pagine libere/occupate
 * - Informazioni sulla memoria del sistema
 * - Hint per ottimizzare le ricerche
 * - Statistiche di utilizzo
 */

// Stato interno del PMM - tutte le informazioni necessarie per gestire la memoria fisica
typedef struct {
  bool initialized;   // Flag: PMM è stato inizializzato?
  u8 *bitmap;         // Puntatore al bitmap (1 bit per pagina)
  u64 bitmap_size;    // Dimensione del bitmap in byte
  u64 total_pages;    // Numero totale di pagine nel sistema
  u64 usable_pages;   // Pagine effettivamente utilizzabili
  u64 highest_page;   // Numero della pagina con indirizzo più alto
  u64 next_free_hint; // Hint: dove iniziare a cercare la prossima pagina libera

  // Informazioni sulla memoria (cache per evitare ricalcoli)
  u64 total_memory;  // Memoria totale del sistema in byte
  u64 usable_memory; // Memoria utilizzabile dal kernel in byte
} pmm_state_t;

// Statistiche globali del PMM - vengono aggiornate ad ogni operazione
static pmm_stats_t pmm_stats;

// Stato globale del PMM - una sola istanza per tutto il kernel
static pmm_state_t pmm_state = {.initialized = false};

// Riferimento esterno alla memory map di Limine (definita in memory.c)
extern volatile struct limine_memmap_request memmap_request;

/*
 * MACRO PER OPERAZIONI SUL BITMAP
 *
 * Il bitmap usa 1 bit per pagina:
 * - Bit = 0: pagina libera
 * - Bit = 1: pagina occupata
 *
 * Per accedere al bit N:
 * - Byte index = N / 8
 * - Bit offset = N % 8
 */
#define BITMAP_SET_BIT(bitmap, bit) ((bitmap)[(bit) / 8] |= (1 << ((bit) % 8)))
#define BITMAP_CLEAR_BIT(bitmap, bit) ((bitmap)[(bit) / 8] &= ~(1 << ((bit) % 8)))
#define BITMAP_TEST_BIT(bitmap, bit) ((bitmap)[(bit) / 8] & (1 << ((bit) % 8)))

/*
 * ESEMPIO DI COME FUNZIONA IL BITMAP:
 *
 * Supponiamo di avere 16 pagine (per semplicità):
 * Bitmap = [0xFF, 0x0F] = [11111111, 00001111]
 *
 * Pagina 0: bit 0 di byte 0 = 1 (occupata)
 * Pagina 1: bit 1 di byte 0 = 1 (occupata)
 * ...
 * Pagina 7: bit 7 di byte 0 = 1 (occupata)
 * Pagina 8: bit 0 di byte 1 = 1 (occupata)
 * ...
 * Pagina 11: bit 3 di byte 1 = 1 (occupata)
 * Pagina 12: bit 4 di byte 1 = 0 (libera)
 * ...
 */

// Dichiarazioni delle funzioni interne (implementate nei passi successivi)
static pmm_result_t pmm_analyze_memory_map(void);
static pmm_result_t pmm_find_bitmap_location(void);
static void pmm_init_bitmap(void);
static void pmm_mark_memory_regions(void);
static void pmm_mark_page_used(u64 page_index);
static void pmm_mark_page_free(u64 page_index);
static bool pmm_is_page_used_internal(u64 page_index);
static void pmm_update_free_pages_count(void);

/*
 * SPIEGAZIONE DELL'ARCHITETTURA:
 *
 * 1. Il PMM divide tutta la memoria fisica in pagine da 4KB
 * 2. Ogni pagina ha un numero (indice): 0, 1, 2, 3...
 * 3. Il bitmap ha 1 bit per ogni pagina per tracciarne lo stato
 * 4. Per 1GB di RAM servono ~32KB di bitmap (1GB / 4KB / 8 bit)
 * 5. Il bitmap stesso viene posizionato in una regione usabile
 */

/*
 * CONVERSIONE TIPI LIMINE E ANALISI MEMORY MAP
 */

/**
 * @brief Converte i tipi di memoria di Limine ai nostri tipi interni
 *
 * Limine usa dei numeri per identificare i tipi di memoria.
 * Noi li convertiamo ai nostri enum per maggiore chiarezza.
 *
 * @param limine_type Tipo di memoria secondo Limine
 * @return Il nostro tipo di memoria corrispondente
 */
static memory_type_t pmm_convert_limine_type(uint64_t limine_type) {
  switch (limine_type) {
  case LIMINE_MEMMAP_USABLE: // = 0
    return MEMORY_USABLE;    // RAM libera, può essere usata

  case LIMINE_MEMMAP_RESERVED: // = 1
    return MEMORY_RESERVED;    // Riservata dal firmware/hardware

  case LIMINE_MEMMAP_ACPI_RECLAIMABLE: // = 2
    return MEMORY_ACPI_RECLAIMABLE;    // Tabelle ACPI, recuperabile dopo lettura

  case LIMINE_MEMMAP_ACPI_NVS: // = 3
    return MEMORY_ACPI_NVS;    // ACPI non-volatile, mai toccabile

  case LIMINE_MEMMAP_BAD_MEMORY: // = 4
    return MEMORY_BAD;           // RAM difettosa

  case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: // = 5
    return MEMORY_BOOTLOADER_RECLAIMABLE;    // Usata da Limine, recuperabile

  case LIMINE_MEMMAP_KERNEL_AND_MODULES:  // = 6
    return MEMORY_EXECUTABLE_AND_MODULES; // Il nostro kernel

  case LIMINE_MEMMAP_FRAMEBUFFER: // = 7
    return MEMORY_FRAMEBUFFER;    // Memoria video

  default:
    return MEMORY_RESERVED; // Se non riconosciuto = riservato
  }
}

/**
 * @brief Analizza la memory map di Limine per capire la struttura della memoria
 *
 * Questa funzione fa la "ricognizione" della memoria:
 * 1. Legge tutte le regioni dalla memory map di Limine
 * 2. Calcola quanta memoria totale abbiamo
 * 3. Trova la pagina con indirizzo più alto
 * 4. Conta quanta memoria è utilizzabile
 *
 * È come fare un censimento della memoria disponibile.
 *
 * @return PMM_SUCCESS se l'analisi è riuscita, errore altrimenti
 */
static pmm_result_t pmm_analyze_memory_map(void) __attribute__((unused));
static pmm_result_t pmm_analyze_memory_map(void) {
  // Verifica che Limine ci abbia dato la memory map
  if (!memmap_request.response) {
    klog_error("PMM: Memory map non disponibile da Limine");
    return PMM_NOT_INITIALIZED;
  }

  struct limine_memmap_response *response = memmap_request.response;

  // Inizializza i contatori
  pmm_state.total_memory = 0;
  pmm_state.usable_memory = 0;
  pmm_state.highest_page = 0;

  klog_info("PMM: Analisi memory map (%lu regioni)", response->entry_count);

  /*
   * PRIMA PASSATA: Scopri i limiti della memoria
   *
   * Iteriamo attraverso tutte le regioni di memoria per:
   * - Sommare la memoria totale
   * - Trovare l'indirizzo più alto (per dimensionare il bitmap)
   * - Contare quanta memoria è utilizzabile
   */
  for (u64 i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    memory_type_t type = pmm_convert_limine_type(entry->type);

    // Accumula memoria totale
    pmm_state.total_memory += entry->length;

    /*
     * CALCOLO PAGINA PIÙ ALTA:
     *
     * Se una regione va da 0x100000 a 0x200000:
     * - end_addr = 0x100000 + 0x100000 = 0x200000
     * - end_page = 0x200000 >> 12 = 512
     *
     * Questo ci dice che abbiamo bisogno di almeno 513 bit nel bitmap
     * (pagine 0-512 inclusive).
     */
    u64 end_addr = entry->base + entry->length;
    u64 end_page = ADDR_TO_PAGE(end_addr);
    if (end_page > pmm_state.highest_page) {
      pmm_state.highest_page = end_page;
    }

    // Conta memoria utilizzabile (solo regioni USABLE per ora)
    if (type == MEMORY_USABLE) {
      pmm_state.usable_memory += entry->length;
    }

    klog_debug("PMM: Regione %lu: 0x%016lx-0x%016lx (%lu MB) tipo=%d", i, entry->base, end_addr - 1, entry->length / MB, type);
  }

  /*
   * CALCOLI FINALI:
   *
   * - total_pages: quante pagine totali dobbiamo gestire
   * - usable_pages: quante di queste sono utilizzabili
   */
  pmm_state.total_pages = pmm_state.highest_page + 1;
  pmm_state.usable_pages = pmm_state.usable_memory / PAGE_SIZE;

  klog_info("PMM: Memoria totale: %lu MB (%lu pagine)", pmm_state.total_memory / MB, pmm_state.total_pages);
  klog_info("PMM: Memoria usabile: %lu MB (%lu pagine)", pmm_state.usable_memory / MB, pmm_state.usable_pages);
  klog_info("PMM: Pagina più alta: %lu (0x%lx)", pmm_state.highest_page, PAGE_TO_ADDR(pmm_state.highest_page));

  return PMM_SUCCESS;
}

/*
 * ESEMPIO PRATICO:
 *
 * Su un sistema con 512MB di RAM, potremmo avere:
 *
 * Regione 0: 0x000000-0x09FFFF (640KB)   - USABLE
 * Regione 1: 0x0A0000-0x0FFFFF (384KB)   - RESERVED (VGA, BIOS)
 * Regione 2: 0x100000-0x1FFFFFF (31MB)   - USABLE
 * Regione 3: 0x2000000-0x20000FF (256B)  - KERNEL_AND_MODULES (nostro kernel)
 * Regione 4: 0x2000100-0x1FFFFFFF (480MB) - USABLE
 *
 * Risultato:
 * - total_memory = 512MB
 * - usable_memory = 640KB + 31MB + 480MB = ~511MB
 * - highest_page = 0x20000000 >> 12 = 131072
 * - total_pages = 131073
 */

/*
 * POSIZIONAMENTO DEL BITMAP
 */

/**
 * @brief Trova una posizione adatta per il bitmap nella memoria usabile
 *
 * Il bitmap è la struttura dati principale del PMM. Dobbiamo posizionarlo
 * da qualche parte nella memoria fisica, preferibilmente in una regione USABLE.
 *
 * REQUISITI DEL BITMAP:
 * - 1 bit per ogni pagina del sistema
 * - Deve essere in memoria accessibile al kernel
 * - Deve essere in una regione che non verrà sovrascritta
 *
 * CALCOLO DIMENSIONE:
 * Per N pagine, servono N bit = N/8 byte (arrotondato in su)
 * Esempio: 131072 pagine = 131072/8 = 16384 byte = 16KB
 *
 * @return PMM_SUCCESS se trova una posizione, PMM_OUT_OF_MEMORY altrimenti
 */
static pmm_result_t pmm_find_bitmap_location(void) __attribute__((unused));
static pmm_result_t pmm_find_bitmap_location(void) {
  if (!memmap_request.response) {
    return PMM_NOT_INITIALIZED;
  }

  /*
   * CALCOLO DIMENSIONE BITMAP:
   *
   * Se abbiamo total_pages pagine, ci servono total_pages bit.
   * Siccome 1 byte = 8 bit, ci servono total_pages/8 byte.
   * Usiamo (total_pages + 7) / 8 per arrotondare in su.
   *
   * Esempio:
   * - 1000 pagine: (1000 + 7) / 8 = 1007 / 8 = 125 byte
   * - 1001 pagine: (1001 + 7) / 8 = 1008 / 8 = 126 byte
   */
  pmm_state.bitmap_size = (pmm_state.total_pages + 7) / 8;

  /*
   * CALCOLO PAGINE NECESSARIE:
   *
   * Il bitmap stesso occuperà delle pagine. Dobbiamo sapere quante
   * per poi marcarle come occupate nel bitmap stesso.
   */
  u64 bitmap_pages_needed = PAGE_ALIGN_UP(pmm_state.bitmap_size) / PAGE_SIZE;

  klog_info("PMM: Bitmap richiesto: %lu bytes (%lu pagine)", pmm_state.bitmap_size, bitmap_pages_needed);

  struct limine_memmap_response *response = memmap_request.response;

  /*
   * RICERCA POSIZIONE ADATTA:
   *
   * Cerchiamo una regione USABLE abbastanza grande per contenere il bitmap.
   * Preferiamo regioni all'inizio della memoria per semplicità.
   */
  for (u64 i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    memory_type_t type = pmm_convert_limine_type(entry->type);

    /*
     * VERIFICA CANDIDATO:
     *
     * 1. Deve essere USABLE
     * 2. Deve essere abbastanza grande
     * 3. Dobbiamo poter allineare l'indirizzo alla pagina
     */
    if (type == MEMORY_USABLE && entry->length >= pmm_state.bitmap_size) {
      /*
       * ALLINEAMENTO ALLA PAGINA:
       *
       * Il bitmap deve iniziare su un boundary di pagina per semplicità.
       * Se la regione inizia a 0x100100, allineiamo a 0x101000.
       */
      u64 aligned_base = PAGE_ALIGN_UP(entry->base);
      u64 available_space = entry->base + entry->length - aligned_base;

      /*
       * VERIFICA SPAZIO SUFFICIENTE:
       *
       * Dopo l'allineamento, verifichiamo che ci sia ancora
       * abbastanza spazio per il bitmap.
       */
      if (available_space >= pmm_state.bitmap_size) {
        pmm_state.bitmap = (u8 *)aligned_base;
        pmm_stats.bitmap_pages = bitmap_pages_needed;

        klog_info("PMM: Bitmap posizionato a 0x%lx (%lu KB)", aligned_base, pmm_state.bitmap_size / 1024);

        return PMM_SUCCESS;
      }
    }
  }

  /*
   * FALLIMENTO:
   *
   * Se arriviamo qui, non abbiamo trovato nessuna regione abbastanza grande.
   * Questo può accadere se:
   * - C'è poca memoria libera
   * - La memoria è molto frammentata
   * - Il bitmap richiesto è troppo grande
   */
  klog_error("PMM: Impossibile trovare spazio per bitmap (%lu bytes richiesti)", pmm_state.bitmap_size);
  return PMM_OUT_OF_MEMORY;
}

/*
 * ESEMPIO PRATICO:
 *
 * Sistema con 4GB di RAM:
 * - total_pages = 4GB / 4KB = 1,048,576 pagine
 * - bitmap_size = 1,048,576 / 8 = 131,072 byte = 128KB
 * - bitmap_pages_needed = 128KB / 4KB = 32 pagine
 *
 * Supponiamo memory map:
 * Regione 0: 0x100000-0x7FFFFFFF (2GB-1MB) USABLE
 *
 * Processo:
 * 1. aligned_base = PAGE_ALIGN_UP(0x100000) = 0x100000 (già allineato)
 * 2. available_space = 0x7FFFFFFF - 0x100000 = ~2GB (più che sufficiente)
 * 3. bitmap posizionato a 0x100000
 * 4. Il bitmap occuperà 0x100000-0x11FFFF (128KB)
 *
 * Risultato: bitmap a 0x100000, occupa 32 pagine
 */

/*
 * INIZIALIZZAZIONE DEL BITMAP
 */

/**
 * @brief Inizializza il bitmap marcando tutte le pagine come occupate
 *
 * FILOSOFIA "SICUREZZA PRIMA":
 *
 * Per sicurezza, inizializziamo tutto il bitmap a 1 (pagine occupate).
 * Questo significa che inizialmente consideriamo TUTTA la memoria come occupata.
 *
 * Successivamente, andremo a marcare come libere SOLO le pagine che sappiamo
 * essere sicuramente utilizzabili. Questo approccio conservativo previene
 * accessi accidentali a memoria riservata.
 *
 * ALTERNATIVA PERICOLOSA:
 * Potremmo inizializzare a 0 (tutto libero) e poi marcare come occupate
 * le aree riservate, ma questo è rischioso: se ci dimentichiamo una regione
 * riservata, potremmo sovrascrivere dati critici.
 */
static void pmm_init_bitmap(void) __attribute__((unused));
static void pmm_init_bitmap(void) {
  /*
   * INIZIALIZZAZIONE A "TUTTO OCCUPATO":
   *
   * memset(bitmap, 0xFF, size) imposta tutti i bit a 1.
   * 0xFF = 11111111 in binario = tutti i bit settati.
   *
   * Dopo questa operazione:
   * - Tutte le pagine risultano "occupate"
   * - Nessuna pagina può essere allocata accidentalmente
   * - Dobbiamo esplicitamente marcare come libere le pagine utilizzabili
   */
  memset(pmm_state.bitmap, 0xFF, pmm_state.bitmap_size);

  /*
   * INIZIALIZZAZIONE STATISTICHE:
   *
   * Le statistiche riflettono lo stato corrente del bitmap.
   * Inizialmente tutte le pagine sono "occupate".
   */
  pmm_stats.total_pages = pmm_state.total_pages;
  pmm_stats.free_pages = 0;                     // Nessuna pagina libera inizialmente
  pmm_stats.used_pages = pmm_state.total_pages; // Tutte considerate occupate
  pmm_stats.reserved_pages = 0;                 // Calcolato dopo
  pmm_stats.alloc_count = 0;                    // Nessuna allocazione ancora
  pmm_stats.free_count = 0;                     // Nessuna deallocazione ancora
  pmm_stats.largest_free_run = 0;               // Nessun blocco libero

  klog_debug("PMM: Bitmap inizializzato - %lu pagine marcate come occupate", pmm_state.total_pages);
}

/*
 * ESEMPIO VISIVO DEL BITMAP:
 *
 * Supponiamo 16 pagine per semplicità:
 *
 * DOPO pmm_init_bitmap():
 *
 * Pagine: 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
 * Bitmap: 1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
 * Byte:   [11111111] [11111111]
 *         = [0xFF]   [0xFF]
 *
 * Significato:
 * - Pagina 0: occupata (bit 0 = 1)
 * - Pagina 1: occupata (bit 1 = 1)
 * - ...
 * - Pagina 15: occupata (bit 7 del byte 1 = 1)
 *
 * Questo è il punto di partenza "sicuro". Successivamente andremo a
 * "liberare" esplicitamente le pagine che sappiamo essere utilizzabili.
 */

/*
 * FUNZIONI DI MANIPOLAZIONE BITMAP
 *
 * Queste sono le funzioni fondamentali per manipolare il bitmap.
 * Sono il "cuore" del PMM - tutto il resto si basa su queste operazioni.
 */

/**
 * @brief Marca una pagina come occupata nel bitmap
 *
 * OPERAZIONE: Imposta il bit corrispondente alla pagina a 1.
 *
 * DETTAGLI IMPLEMENTATIVI:
 * - Il bit della pagina N si trova nel byte N/8, posizione N%8
 * - Usiamo OR bitwise (|=) per settare il bit senza alterare gli altri
 *
 * @param page_index Indice della pagina da marcare come occupata
 */
static void pmm_mark_page_used(u64 page_index) {
  // Verifica bounds per evitare buffer overflow
  if (page_index < pmm_state.total_pages) {
    BITMAP_SET_BIT(pmm_state.bitmap, page_index);
  }
}

/**
 * @brief Marca una pagina come libera nel bitmap
 *
 * OPERAZIONE: Imposta il bit corrispondente alla pagina a 0.
 *
 * DETTAGLI IMPLEMENTATIVI:
 * - Usiamo AND bitwise (&=) con la negazione per cancellare solo quel bit
 * - ~(1 << (bit%8)) crea una maschera con tutti i bit a 1 eccetto quello target
 *
 * @param page_index Indice della pagina da marcare come libera
 */
static void pmm_mark_page_free(u64 page_index) {
  // Verifica bounds per evitare buffer overflow
  if (page_index < pmm_state.total_pages) {
    BITMAP_CLEAR_BIT(pmm_state.bitmap, page_index);
  }
}

/**
 * @brief Verifica se una pagina è occupata
 *
 * OPERAZIONE: Legge il bit corrispondente alla pagina.
 *
 * SICUREZZA: Pagine fuori range sono considerate sempre occupate
 * per prevenire accessi a memoria non valida.
 *
 * @param page_index Indice della pagina da verificare
 * @return true se la pagina è occupata, false se è libera
 */
static bool pmm_is_page_used_internal(u64 page_index) {
  // Pagine fuori range = sempre occupate (sicurezza)
  if (page_index >= pmm_state.total_pages) {
    return true;
  }

  // Testa il bit: se != 0, la pagina è occupata
  return BITMAP_TEST_BIT(pmm_state.bitmap, page_index) != 0;
}

/*
 * ESEMPIO STEP-BY-STEP DELLE OPERAZIONI:
 *
 * Stato iniziale (8 pagine per semplicità):
 * Bitmap: [11111111] = 0xFF
 * Pagine: 7 6 5 4 3 2 1 0 (ordine dei bit nel byte)
 *         1 1 1 1 1 1 1 1 (tutte occupate)
 *
 * 1. pmm_mark_page_free(3):
 *    - Byte index = 3/8 = 0
 *    - Bit offset = 3%8 = 3
 *    - Maschera = ~(1 << 3) = ~00001000 = 11110111
 *    - bitmap[0] &= 11110111
 *    - Risultato: [11110111] = 0xF7
 *
 * 2. pmm_mark_page_free(5):
 *    - Byte index = 5/8 = 0
 *    - Bit offset = 5%8 = 5
 *    - Maschera = ~(1 << 5) = ~00100000 = 11011111
 *    - bitmap[0] &= 11011111
 *    - Risultato: [11010111] = 0xD7
 *
 * 3. pmm_is_page_used_internal(3):
 *    - Test bit 3: bitmap[0] & 00001000 = 11010111 & 00001000 = 0
 *    - Ritorna false (pagina libera)
 *
 * 4. pmm_is_page_used_internal(4):
 *    - Test bit 4: bitmap[0] & 00010000 = 11010111 & 00010000 = 00010000
 *    - Ritorna true (pagina occupata)
 *
 * Stato finale:
 * Bitmap: [11010111] = 0xD7
 * Pagine: 7 6 5 4 3 2 1 0
 *         1 1 0 1 0 1 1 1
 *         ↑ ↑ ↑ ↑ ↑ ↑ ↑ ↑
 *         | | | | | | | +-- Pagina 0: occupata
 *         | | | | | | +-- Pagina 1: occupata
 *         | | | | | +-- Pagina 2: occupata
 *         | | | | +-- Pagina 3: LIBERA ✓
 *         | | | +-- Pagina 4: occupata
 *         | | +-- Pagina 5: LIBERA ✓
 *         | +-- Pagina 6: occupata
 *         +-- Pagina 7: occupata
 *
 * PERFORMANCE NOTE:
 * Queste operazioni sono O(1) - costante, molto veloci!
 * L'accesso a un singolo bit richiede:
 * 1. Una divisione/modulo (o shift/mask)
 * 2. Un accesso a memoria
 * 3. Un'operazione bitwise
 */

/*
 * MARCATURA DELLE REGIONI DI MEMORIA
 *
 * Ora che abbiamo il bitmap inizializzato (tutto occupato), dobbiamo
 * andare regione per regione e marcare come LIBERE quelle che possiamo usare.
 */

/**
 * @brief Marca le regioni di memoria secondo il loro tipo
 *
 * PROCESSO:
 * 1. Iterare attraverso tutte le regioni dalla memory map di Limine
 * 2. Per ogni regione, determinare se è utilizzabile o riservata
 * 3. Marcare le pagine corrispondenti nel bitmap
 * 4. Aggiornare le statistiche
 *
 * POLITICA DI UTILIZZO:
 * - USABLE: Immediatamente utilizzabile → marca come libero
 * - BOOTLOADER_RECLAIMABLE: Utilizzabile dopo init → marca come libero
 * - ACPI_RECLAIMABLE: Utilizzabile dopo lettura tabelle → marca come libero
 * - Tutto il resto: Riservato → lascia occupato
 */
static void pmm_mark_memory_regions(void) __attribute__((unused));
static void pmm_mark_memory_regions(void) {
  struct limine_memmap_response *response = memmap_request.response;

  klog_info("PMM: Marcatura regioni di memoria...");

  /*
   * ITERAZIONE ATTRAVERSO TUTTE LE REGIONI:
   *
   * Per ogni regione nella memory map, determiniamo le pagine
   * che copre e le marchiamo secondo il tipo.
   */
  for (u64 i = 0; i < response->entry_count; i++) {
    struct limine_memmap_entry *entry = response->entries[i];
    memory_type_t type = pmm_convert_limine_type(entry->type);

    /*
     * CALCOLO RANGE DI PAGINE:
     *
     * Una regione va da entry->base a entry->base + entry->length.
     * Dobbiamo convertire questi indirizzi in numeri di pagina.
     *
     * Esempio:
     * - Regione: 0x100000-0x200000 (1MB-2MB)
     * - start_page = 0x100000 >> 12 = 256
     * - end_addr = 0x100000 + 0x100000 = 0x200000
     * - end_page = (0x200000 - 1) >> 12 = 511
     * - page_count = 511 - 256 + 1 = 256 pagine
     */
    u64 start_page = ADDR_TO_PAGE(entry->base);
    u64 end_page = ADDR_TO_PAGE(entry->base + entry->length - 1);
    u64 page_count = end_page - start_page + 1;

    /*
     * MARCATURA SECONDO IL TIPO:
     *
     * Diversi tipi di memoria richiedono trattamenti diversi.
     */
    switch (type) {
    case MEMORY_USABLE:
      /*
       * MEMORIA USABILE:
       *
       * Questa è RAM che possiamo usare liberamente.
       * Marchiamo tutte le pagine come libere.
       */
      for (u64 page = start_page; page <= end_page; page++) {
        pmm_mark_page_free(page);
      }
      klog_debug("PMM: Marcate %lu pagine usabili (0x%lx-0x%lx)", page_count, PAGE_TO_ADDR(start_page), PAGE_TO_ADDR(end_page));
      break;

    case MEMORY_BOOTLOADER_RECLAIMABLE:
      /*
       * MEMORIA BOOTLOADER RECLAIMABLE:
       *
       * Questa memoria è usata da Limine durante il boot, ma
       * può essere recuperata una volta che il kernel è attivo.
       * La marchiamo come libera.
       */
      for (u64 page = start_page; page <= end_page; page++) {
        pmm_mark_page_free(page);
      }
      klog_debug("PMM: Marcate %lu pagine bootloader reclaimable", page_count);
      break;

    case MEMORY_ACPI_RECLAIMABLE:
      /*
       * MEMORIA ACPI RECLAIMABLE:
       *
       * Contiene tabelle ACPI che il kernel deve leggere una volta.
       * Dopo la lettura, questa memoria può essere recuperata.
       * La marchiamo come libera.
       */
      for (u64 page = start_page; page <= end_page; page++) {
        pmm_mark_page_free(page);
      }
      klog_debug("PMM: Marcate %lu pagine ACPI reclaimable", page_count);
      break;

    case MEMORY_EXECUTABLE_AND_MODULES:
    case MEMORY_RESERVED:
    case MEMORY_ACPI_NVS:
    case MEMORY_BAD:
    case MEMORY_FRAMEBUFFER:
    default:
      /*
       * MEMORIA RISERVATA:
       *
       * Queste regioni non possono essere usate:
       * - EXECUTABLE_AND_MODULES: Il nostro kernel (non sovrascrivere!)
       * - RESERVED: Riservata dal firmware/hardware
       * - ACPI_NVS: Tabelle ACPI permanenti
       * - BAD: RAM difettosa
       * - FRAMEBUFFER: Memoria video
       *
       * Le lasciamo marcate come occupate (sono già così dal init).
       */
      pmm_stats.reserved_pages += page_count;
      klog_debug("PMM: %lu pagine riservate/occupate per tipo %d", page_count, type);
      break;
    }
  }

  /*
   * PROTEZIONE DEL BITMAP STESSO:
   *
   * Il bitmap stesso occupa della memoria fisica. Dobbiamo marcare
   * quelle pagine come occupate per evitare di sovrascrivere il bitmap!
   */
  u64 bitmap_start_page = ADDR_TO_PAGE((u64)pmm_state.bitmap);
  u64 bitmap_end_page = ADDR_TO_PAGE((u64)pmm_state.bitmap + pmm_state.bitmap_size - 1);

  for (u64 page = bitmap_start_page; page <= bitmap_end_page; page++) {
    pmm_mark_page_used(page);
  }

  klog_info("PMM: Bitmap occupa pagine %lu-%lu (%lu pagine)", bitmap_start_page, bitmap_end_page, pmm_stats.bitmap_pages);

  /*
   * AGGIORNAMENTO STATISTICHE FINALI:
   *
   * Ora che abbiamo marcato tutte le regioni, aggiorniamo
   * le statistiche contando effettivamente le pagine libere/occupate.
   */
  pmm_update_free_pages_count();

  klog_info("PMM: Marcatura completata - %lu libere, %lu occupate, %lu riservate", pmm_stats.free_pages, pmm_stats.used_pages, pmm_stats.reserved_pages);
}

/**
 * @brief Aggiorna il conteggio delle pagine libere/occupate contando il bitmap
 *
 * Questa funzione "conta" effettivamente i bit nel bitmap per aggiornare
 * le statistiche. È più lenta delle operazioni incrementali, ma garantisce
 * accuratezza.
 */
static void pmm_update_free_pages_count(void) {
  u64 free_count = 0;
  u64 used_count = 0;

  /*
   * CONTEGGIO COMPLETO:
   *
   * Iteriamo attraverso tutte le pagine e contiamo quelle
   * libere e quelle occupate.
   */
  for (u64 i = 0; i < pmm_state.total_pages; i++) {
    if (pmm_is_page_used_internal(i)) {
      used_count++;
    } else {
      free_count++;
    }
  }

  pmm_stats.free_pages = free_count;
  pmm_stats.used_pages = used_count;
}

/*
 * ESEMPIO PRATICO DELLA MARCATURA:
 *
 * Supponiamo un sistema semplice con questa memory map:
 *
 * Regione 0: 0x000000-0x0FFFFF (1MB)     - RESERVED (legacy area)
 * Regione 1: 0x100000-0x1FFFFF (1MB)     - USABLE
 * Regione 2: 0x200000-0x20FFFF (64KB)    - KERNEL_AND_MODULES (nostro kernel)
 * Regione 3: 0x210000-0x7FFFFFF (~126MB) - USABLE
 * Regione 4: 0x8000000-0x8FFFFFF (16MB)  - BOOTLOADER_RECLAIMABLE
 *
 * Processo di marcatura:
 *
 * 1. Inizialmente: tutte le pagine sono marcate come occupate
 *
 * 2. Regione 0 (RESERVED): lasciamo occupate (pagine 0-255)
 *
 * 3. Regione 1 (USABLE): marchiamo libere (pagine 256-511)
 *    Bitmap: bit 256-511 → 0
 *
 * 4. Regione 2 (KERNEL): lasciamo occupate (pagine 512-527)
 *
 * 5. Regione 3 (USABLE): marchiamo libere (pagine 528-32767)
 *    Bitmap: bit 528-32767 → 0
 *
 * 6. Regione 4 (BOOTLOADER_RECLAIMABLE): marchiamo libere (pagine 32768-36863)
 *    Bitmap: bit 32768-36863 → 0
 *
 * 7. Protezione bitmap: se il bitmap è a 0x100000, marchiamo occupate le
 *    pagine che contengono il bitmap stesso
 *
 * Risultato finale:
 * - Pagine libere: ~32500 (circa 127MB utilizzabili)
 * - Pagine occupate: ~4200 (legacy area + kernel + bitmap)
 * - Pagine riservate: incluse nel conteggio "occupate"
 */

/*
 * IMPLEMENTAZIONI DELLE FUNZIONI PUBBLICHE
 * Queste sono le funzioni che il kernel userà per interagire con il PMM.
 */

/**
 * @brief Inizializza il Physical Memory Manager
 */
pmm_result_t pmm_init(void) {
  klog_info("PMM: Inizializzazione Physical Memory Manager...");

  // DEBUG: Verifica memmap_request
  if (!memmap_request.response) {
    klog_error("PMM: memmap_request.response è NULL!");
    return PMM_NOT_INITIALIZED;
  }
  klog_info("PMM: memmap_request OK, %lu regioni disponibili", memmap_request.response->entry_count);

  // 1. Analizza la memory map di Limine
  klog_info("PMM: Fase 1 - Analisi memory map...");
  pmm_result_t result = pmm_analyze_memory_map();
  if (result != PMM_SUCCESS) {
    klog_error("PMM: ERRORE nell'analisi della memory map: codice %d", result);
    return result;
  }
  klog_info("PMM: Fase 1 completata - %lu pagine totali, %lu utilizzabili", pmm_state.total_pages, pmm_state.usable_pages);

  // 2. Trova una posizione per il bitmap
  klog_info("PMM: Fase 2 - Posizionamento bitmap...");
  result = pmm_find_bitmap_location();
  if (result != PMM_SUCCESS) {
    klog_error("PMM: ERRORE nel posizionamento del bitmap: codice %d", result);
    return result;
  }
  klog_info("PMM: Fase 2 completata - Bitmap a 0x%lx, dimensione %lu bytes", (u64)pmm_state.bitmap, pmm_state.bitmap_size);

  // 3. Inizializza il bitmap (tutto occupato)
  klog_info("PMM: Fase 3 - Inizializzazione bitmap...");
  pmm_init_bitmap();
  klog_info("PMM: Fase 3 completata - %lu pagine marcate come occupate", pmm_state.total_pages);

  // 4. Marca le regioni secondo il loro tipo
  klog_info("PMM: Fase 4 - Marcatura regioni memoria...");
  pmm_mark_memory_regions();
  klog_info("PMM: Fase 4 completata - %lu libere, %lu occupate", pmm_stats.free_pages, pmm_stats.used_pages);

  // 5. Imposta l'hint per le ricerche
  pmm_state.next_free_hint = 0;
  klog_info("PMM: Hint iniziale impostato a: %lu", pmm_state.next_free_hint);

  // DEBUG: Test immediato bitmap
  klog_info("PMM: Test immediato bitmap...");
  u64 test_free_count = 0;
  for (u64 i = 0; i < 100 && i < pmm_state.total_pages; i++) {
    if (!pmm_is_page_used_internal(i)) {
      test_free_count++;
      if (test_free_count == 1) {
        klog_info("PMM: Prima pagina libera trovata: %lu", i);
      }
    }
  }
  klog_info("PMM: Test primi 100 bit - pagine libere trovate: %lu", test_free_count);

  // 6. Marca come inizializzato
  pmm_state.initialized = true;

  klog_info("PMM: Inizializzazione completata con successo");
  klog_info("PMM: %lu MB totali, %lu MB utilizzabili", pmm_state.total_memory / MB, pmm_state.usable_memory / MB);

  pmm_mark_page_used(0); // Riserva sempre la pagina 0 - Altrimenti potrebbe causare problemi con i puntatori NULL durante la prima allocazione
  klog_debug("PMM: Pagina 0 riservata (NULL pointer protection)");

  return PMM_SUCCESS;
}

/**
 * @brief Trova la prima pagina libera a partire da un hint
 */
static u64 pmm_find_free_page_from(u64 start_hint) {
  for (u64 i = start_hint; i < pmm_state.total_pages; i++) {
    if (!pmm_is_page_used_internal(i)) {
      return i;
    }
  }

  // Se non trovato dopo l'hint, cerca dall'inizio
  for (u64 i = 0; i < start_hint; i++) {
    if (!pmm_is_page_used_internal(i)) {
      return i;
    }
  }

  return pmm_state.total_pages; // Non trovato
}

/**
 * @brief Trova un blocco di pagine contigue libere
 */
static u64 pmm_find_free_pages_from(u64 start_hint, size_t count) {
  for (u64 start = start_hint; start + count <= pmm_state.total_pages; start++) {
    bool found = true;

    // Verifica se tutte le pagine del blocco sono libere
    for (size_t i = 0; i < count; i++) {
      if (pmm_is_page_used_internal(start + i)) {
        found = false;
        start += i; // Salta avanti per ottimizzare
        break;
      }
    }

    if (found) {
      return start;
    }
  }

  // Se non trovato dopo l'hint, cerca dall'inizio
  for (u64 start = 0; start < start_hint && start + count <= pmm_state.total_pages; start++) {
    bool found = true;

    for (size_t i = 0; i < count; i++) {
      if (pmm_is_page_used_internal(start + i)) {
        found = false;
        start += i;
        break;
      }
    }

    if (found) {
      return start;
    }
  }

  return pmm_state.total_pages; // Non trovato
}

/**
 * @brief Alloca una singola pagina fisica
 */
void *pmm_alloc_page(void) {
  if (!pmm_state.initialized) {
    return NULL;
  }

  if (pmm_stats.free_pages == 0) {
    return NULL; // Memoria esaurita
  }

  // Trova una pagina libera
  u64 page_index = pmm_find_free_page_from(pmm_state.next_free_hint);

  if (page_index >= pmm_state.total_pages) {
    return NULL; // Non trovata
  }

  // Marca come occupata
  pmm_mark_page_used(page_index);

  // Aggiorna statistiche
  pmm_stats.free_pages--;
  pmm_stats.used_pages++;
  pmm_stats.alloc_count++;

  // Aggiorna hint
  pmm_state.next_free_hint = page_index + 1;

  return (void *)PAGE_TO_ADDR(page_index);
}

/**
 * @brief Alloca multiple pagine fisiche contigue
 */
void *pmm_alloc_pages(size_t count) {
  if (!pmm_state.initialized || count == 0) {
    return NULL;
  }

  if (pmm_stats.free_pages < count) {
    return NULL; // Non abbastanza memoria
  }

  // Trova un blocco contiguo
  u64 start_page = pmm_find_free_pages_from(pmm_state.next_free_hint, count);

  if (start_page >= pmm_state.total_pages) {
    return NULL; // Non trovato
  }

  // Marca tutte le pagine come occupate
  for (size_t i = 0; i < count; i++) {
    pmm_mark_page_used(start_page + i);
  }

  // Aggiorna statistiche
  pmm_stats.free_pages -= count;
  pmm_stats.used_pages += count;
  pmm_stats.alloc_count++;

  // Aggiorna hint
  pmm_state.next_free_hint = start_page + count;

  return (void *)PAGE_TO_ADDR(start_page);
}

/**
 * @brief Libera una singola pagina fisica
 */
pmm_result_t pmm_free_page(void *page) {
  if (!pmm_state.initialized) {
    return PMM_NOT_INITIALIZED;
  }

  if (!page) {
    return PMM_INVALID_ADDRESS;
  }

  u64 addr = (u64)page;

  // Verifica allineamento
  if (addr % PAGE_SIZE != 0) {
    return PMM_INVALID_ADDRESS;
  }

  u64 page_index = ADDR_TO_PAGE(addr);

  // Verifica range valido
  if (page_index >= pmm_state.total_pages) {
    return PMM_INVALID_ADDRESS;
  }

  // Verifica se è già libera
  if (!pmm_is_page_used_internal(page_index)) {
    return PMM_ALREADY_FREE;
  }

  // Marca come libera
  pmm_mark_page_free(page_index);

  // Aggiorna statistiche
  pmm_stats.free_pages++;
  pmm_stats.used_pages--;
  pmm_stats.free_count++;

  // Aggiorna hint se necessario
  if (page_index < pmm_state.next_free_hint) {
    pmm_state.next_free_hint = page_index;
  }

  return PMM_SUCCESS;
}

/**
 * @brief Libera multiple pagine fisiche contigue
 */
pmm_result_t pmm_free_pages(void *pages, size_t count) {
  if (!pmm_state.initialized) {
    return PMM_NOT_INITIALIZED;
  }

  if (!pages || count == 0) {
    return PMM_INVALID_ADDRESS;
  }

  u64 addr = (u64)pages;

  // Verifica allineamento
  if (addr % PAGE_SIZE != 0) {
    return PMM_INVALID_ADDRESS;
  }

  u64 start_page = ADDR_TO_PAGE(addr);

  // Verifica range valido
  if (start_page + count > pmm_state.total_pages) {
    return PMM_INVALID_ADDRESS;
  }

  // Verifica che tutte le pagine siano attualmente occupate
  for (size_t i = 0; i < count; i++) {
    if (!pmm_is_page_used_internal(start_page + i)) {
      return PMM_ALREADY_FREE;
    }
  }

  // Libera tutte le pagine
  for (size_t i = 0; i < count; i++) {
    pmm_mark_page_free(start_page + i);
  }

  // Aggiorna statistiche
  pmm_stats.free_pages += count;
  pmm_stats.used_pages -= count;
  pmm_stats.free_count++;

  // Aggiorna hint se necessario
  if (start_page < pmm_state.next_free_hint) {
    pmm_state.next_free_hint = start_page;
  }

  return PMM_SUCCESS;
}

/**
 * @brief Verifica se una pagina è libera
 */
bool pmm_is_page_free(void *page) {
  if (!pmm_state.initialized || !page) {
    return false;
  }

  u64 addr = (u64)page;

  // Verifica allineamento
  if (addr % PAGE_SIZE != 0) {
    return false;
  }

  u64 page_index = ADDR_TO_PAGE(addr);

  // Verifica range valido
  if (page_index >= pmm_state.total_pages) {
    return false;
  }

  return !pmm_is_page_used_internal(page_index);
}

/**
 * @brief Ottiene le statistiche correnti del PMM
 */
const pmm_stats_t *pmm_get_stats(void) {
  if (!pmm_state.initialized) {
    return NULL;
  }

  return &pmm_stats;
}

/**
 * @brief Stampa informazioni dettagliate sullo stato del PMM
 */
void pmm_print_info(void) {
  if (!pmm_state.initialized) {
    klog_error("PMM: Non inizializzato");
    return;
  }

  klog_info("=== PHYSICAL MEMORY MANAGER INFO ===");
  klog_info("Memoria totale: %lu MB (%lu pagine)", pmm_state.total_memory / MB, pmm_stats.total_pages);
  klog_info("Memoria utilizzabile: %lu MB (%lu pagine)", pmm_state.usable_memory / MB, pmm_state.usable_pages);
  klog_info("Pagine libere: %lu (%lu MB)", pmm_stats.free_pages, pmm_stats.free_pages * PAGE_SIZE / MB);
  klog_info("Pagine occupate: %lu (%lu MB)", pmm_stats.used_pages, pmm_stats.used_pages * PAGE_SIZE / MB);
  klog_info("Pagine riservate: %lu (%lu MB)", pmm_stats.reserved_pages, pmm_stats.reserved_pages * PAGE_SIZE / MB);
  klog_info("Bitmap: %lu pagine (%lu KB)", pmm_stats.bitmap_pages, pmm_state.bitmap_size / 1024);
  klog_info("Allocazioni totali: %lu", pmm_stats.alloc_count);
  klog_info("Deallocazioni totali: %lu", pmm_stats.free_count);

  // Calcola percentuale di utilizzo
  u64 usage_percent = (pmm_stats.used_pages * 100) / pmm_stats.total_pages;
  klog_info("Utilizzo memoria: %lu%%", usage_percent);
}

/**
 * @brief Verifica l'integrità del bitmap (debug)
 */
bool pmm_check_integrity(void) {
  if (!pmm_state.initialized) {
    return false;
  }

  u64 free_count = 0;
  u64 used_count = 0;

  // Conta pagine libere e occupate
  for (u64 i = 0; i < pmm_state.total_pages; i++) {
    if (pmm_is_page_used_internal(i)) {
      used_count++;
    } else {
      free_count++;
    }
  }

  // Verifica consistenza con le statistiche
  bool consistent = (free_count == pmm_stats.free_pages) && (used_count == pmm_stats.used_pages);

  if (!consistent) {
    klog_error("PMM: Inconsistenza rilevata!");
    klog_error("  Contate libere: %lu, Stats: %lu", free_count, pmm_stats.free_pages);
    klog_error("  Contate occupate: %lu, Stats: %lu", used_count, pmm_stats.used_pages);
  }

  return consistent;
}

/**
 * @brief Trova la sequenza più lunga di pagine libere contigue
 */
size_t pmm_find_largest_free_run(size_t *start_page) {
  if (!pmm_state.initialized) {
    return 0;
  }

  size_t max_run = 0;
  size_t current_run = 0;
  size_t max_start = 0;
  size_t current_start = 0;

  for (u64 i = 0; i < pmm_state.total_pages; i++) {
    if (!pmm_is_page_used_internal(i)) {
      if (current_run == 0) {
        current_start = i;
      }
      current_run++;

      if (current_run > max_run) {
        max_run = current_run;
        max_start = current_start;
      }
    } else {
      current_run = 0;
    }
  }

  if (start_page) {
    *start_page = max_start;
  }

  // Aggiorna statistica
  pmm_stats.largest_free_run = max_run;

  return max_run;
}

/**
 * @brief Stampa statistiche di frammentazione dettagliate
 */
void pmm_print_fragmentation_info(void) {
  if (!pmm_state.initialized) {
    return;
  }

  size_t start_page;
  size_t largest_run = pmm_find_largest_free_run(&start_page);

  klog_info("=== FRAMMENTAZIONE MEMORIA ===");
  klog_info("Blocco contiguo più grande: %lu pagine (%lu MB)", largest_run, largest_run * PAGE_SIZE / MB);

  if (largest_run > 0) {
    klog_info("Posizione: pagina %lu (0x%lx)", start_page, PAGE_TO_ADDR(start_page));
  }

  // Calcola frammentazione
  if (pmm_stats.free_pages > 0) {
    u64 fragmentation = 100 - ((largest_run * 100) / pmm_stats.free_pages);
    klog_info("Frammentazione: %lu%%", fragmentation);
  }
}

/**
 * @brief Implementazioni stub per funzioni avanzate
 */
void *pmm_alloc_pages_in_range(size_t count, u64 min_addr, u64 max_addr) {
  // TODO: Implementazione avanzata per allocazioni in range specifici
  (void)count;
  (void)min_addr;
  (void)max_addr;
  return NULL;
}

void *pmm_alloc_aligned(size_t pages, size_t alignment) {
  // TODO: Implementazione per allocazioni allineate
  (void)pages;
  (void)alignment;
  return NULL;
}

bool pmm_get_page_info(void *page, u64 *page_index, bool *is_free) {
  if (!pmm_state.initialized || !page) {
    return false;
  }

  u64 addr = (u64)page;

  if (addr % PAGE_SIZE != 0) {
    return false;
  }

  u64 idx = ADDR_TO_PAGE(addr);

  if (idx >= pmm_state.total_pages) {
    return false;
  }

  if (page_index) {
    *page_index = idx;
  }

  if (is_free) {
    *is_free = !pmm_is_page_used_internal(idx);
  }

  return true;
}