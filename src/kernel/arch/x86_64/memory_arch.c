#include <arch/cpu.h>
#include <arch/memory.h>
#include <bootloader/limine.h>
#include <klib/klog.h>
#include <lib/string.h>
#include <lib/types.h>

/**
 * @file arch/x86_64/memory.c
 * @brief Implementazione x86_64-specifica dell'interfaccia di memoria
 *
 * Questo file implementa tutte le funzioni definite nell'interfaccia arch/memory.h
 * per l'architettura x86_64. Si interfaccia con il bootloader Limine per ottenere
 * informazioni sulla memoria fisica e fornisce validazioni specifiche per x86_64.
 *
 * RESPONSABILITÀ:
 * - Interfacciarsi con Limine per ottenere la memory map
 * - Convertire i tipi Limine ai nostri tipi standard
 * - Validare che le regioni di memoria siano compatibili con x86_64
 * - Fornire statistiche aggregate sulla memoria
 * - Inizializzare il sottosistema memoria specifico per x86_64
 */

/*
 * ============================================================================
 * COSTANTI E DEFINIZIONI x86_64 SPECIFICHE
 * ============================================================================
 */

/**
 * Limiti hardware x86_64:
 * - x86_64 supporta indirizzi fisici fino a 52 bit (4 PB teorici)
 * - In pratica molti processori supportano solo 36-48 bit
 * - La prima pagina (0x0) è sempre riservata per NULL pointer protection
 */
#define X86_64_MAX_PHYSICAL_BITS 52
#define X86_64_MAX_PHYSICAL_ADDR ((1ULL << X86_64_MAX_PHYSICAL_BITS) - 1)
#define X86_64_RESERVED_PAGE_0 0x1000 // Prima pagina sempre riservata

/**
 * Costanti per statistiche e validazione
 */
#define MIN_USABLE_MEMORY_MB 16                    // Minimo 16MB per kernel funzionale
#define MAX_MEMORY_REGIONS ARCH_MAX_MEMORY_REGIONS // Mant. compatibilità

typedef struct {
  u64 base;
  u64 end;
} mem_range_t;

// Intervalli tipici di memoria MMIO riservata su PC
static const mem_range_t mmio_ranges[] = {
    {0x000A0000, 0x000FFFFF}, // VGA/BIOS
    {0xFEC00000, 0xFEEFFFFF}, // IOAPIC/HPET
    {0xFE000000, 0xFEFFFFFF}, // Firmware
};

#define MMIO_RANGE_COUNT (sizeof(mmio_ranges) / sizeof(mmio_ranges[0]))

// Alcune zone comunemente riservate per ACPI/firmware
static const mem_range_t acpi_reserved_ranges[] = {
    {0x000E0000, 0x000FFFFF}, // ACPI RSDP / BIOS
};
#define ACPI_RANGE_COUNT (sizeof(acpi_reserved_ranges) / sizeof(acpi_reserved_ranges[0]))

/*
 * ============================================================================
 * VARIABILI GLOBALI E STATO x86_64
 * ============================================================================
 */

/**
 * @brief Richiesta memory map a Limine (SPOSTATA DA mm/memory.c)
 *
 * Questa è la struttura che Limine popola automaticamente durante il boot
 * con informazioni complete sulla memoria fisica disponibile.
 * È specifica per x86_64 e bootloader Limine.
 */
volatile struct limine_memmap_request memmap_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

/**
 * @brief Cache delle statistiche memoria calcolate
 *
 * Manteniamo una cache delle statistiche per evitare di ricalcolare
 * ogni volta. Viene aggiornata durante detect_regions().
 */
static memory_stats_t cached_stats = {.total_memory = 0, .usable_memory = 0, .reserved_memory = 0, .executable_memory = 0, .largest_free_region = 0};

/**
 * @brief Flag per indicare se le statistiche sono valide
 */
static bool stats_valid = false;

/*
 * ============================================================================
 * FUNZIONI DI CONVERSIONE E UTILITÀ INTERNE
 * ============================================================================
 */

/**
 * @brief Converte tipi di memoria Limine ai nostri tipi standard
 *
 * Limine definisce i suoi tipi numerici per le regioni di memoria.
 * Questa funzione li mappa ai nostri enum standard che sono
 * architecture-independent.
 *
 * @param limine_type Tipo numerico definito da Limine
 * @return Tipo standard memory_type_t corrispondente
 *
 * @note Se il tipo non è riconosciuto, ritorna MEMORY_RESERVED per sicurezza
 */
static memory_type_t convert_limine_type(uint64_t limine_type) {
  switch (limine_type) {
  case LIMINE_MEMMAP_USABLE:
    return MEMORY_USABLE;

  case LIMINE_MEMMAP_RESERVED:
    return MEMORY_RESERVED;

  case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
    return MEMORY_ACPI_RECLAIMABLE;

  case LIMINE_MEMMAP_ACPI_NVS:
    return MEMORY_ACPI_NVS;

  case LIMINE_MEMMAP_BAD_MEMORY:
    return MEMORY_BAD;

  case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
    return MEMORY_BOOTLOADER_RECLAIMABLE;

  case LIMINE_MEMMAP_KERNEL_AND_MODULES:
    return MEMORY_EXECUTABLE_AND_MODULES;

  case LIMINE_MEMMAP_FRAMEBUFFER:
    return MEMORY_FRAMEBUFFER;

  default:
    // Tipo sconosciuto - consideriamo riservato per sicurezza
    klog_warn("x86_64: Tipo memoria Limine sconosciuto: %lu, considerato RESERVED", limine_type);
    return MEMORY_RESERVED;
  }
}

/**
 * @brief Ritorna il nome human-readable di un tipo di memoria
 *
 * Funzione di utilità per logging e debug. Converte il nostro
 * enum memory_type_t in una stringa descrittiva.
 *
 * @param type Tipo di memoria da convertire
 * @return Stringa costante con il nome del tipo
 */
static const char *memory_type_name(memory_type_t type) {
  switch (type) {
  case MEMORY_USABLE:
    return "USABLE";
  case MEMORY_RESERVED:
    return "RESERVED";
  case MEMORY_ACPI_RECLAIMABLE:
    return "ACPI_RECLAIMABLE";
  case MEMORY_ACPI_NVS:
    return "ACPI_NVS";
  case MEMORY_BAD:
    return "BAD";
  case MEMORY_BOOTLOADER_RECLAIMABLE:
    return "BOOTLOADER_RECLAIMABLE";
  case MEMORY_EXECUTABLE_AND_MODULES:
    return "KERNEL_AND_MODULES";
  case MEMORY_FRAMEBUFFER:
    return "FRAMEBUFFER";
  default:
    return "UNKNOWN";
  }
}

void log_memory_type(memory_type_t type) {
  klog_debug("Memory type: %s", memory_type_name(type));
}

// Controllo sovrapposizione intervalli
static bool ranges_overlap(u64 a_start, u64 a_end, u64 b_start, u64 b_end) {
  return !(a_end < b_start || b_end < a_start);
}

/**
 * @brief Valida una singola regione di memoria per x86_64
 *
 * Controlla che una regione rispetti tutti i vincoli hardware e
 * software specifici dell'architettura x86_64.
 *
 * @param region Puntatore alla regione da validare
 * @return true se la regione è valida, false altrimenti
 */
static bool validate_memory_region(const memory_region_t *region) {
  if (!region) {
    klog_debug("validate_memory_region(): regione scartata - base=0x%lx size=0x%lx tipo=%d", region->base, region->length, region->type);

    return false;
  }

  // Controlla overflow nella somma base + length
  if (region->base + region->length < region->base) {
    klog_warn("x86_64: Regione con overflow: base=0x%lx, length=0x%lx", region->base, region->length);
    return false;
  }

  // Controlla che non ecceda i limiti fisici x86_64
  u64 end_addr = region->base + region->length - 1;
  if (end_addr > X86_64_MAX_PHYSICAL_ADDR) {
    klog_warn("x86_64: Regione eccede limiti fisici: end=0x%lx, max=0x%lx", end_addr, X86_64_MAX_PHYSICAL_ADDR);
    return false;
  }

  // Controlla allineamento base (deve essere allineato a pagina)
  if (region->base % PAGE_SIZE != 0) {
    klog_warn("x86_64: Regione non allineata: base=0x%lx", region->base);
    // Non è fatale, ma loggiamo l'avvertimento
  }

  // Controlla dimensione minima sensata (almeno 1 pagina per regioni USABLE)
  if (region->type == MEMORY_USABLE && region->length < PAGE_SIZE) {
    klog_debug("x86_64: Regione USABLE troppo piccola: %lu bytes", region->length);
    // Non blocchiamo, ma potrebbe non essere utile
  }

  // Verifica conflitto con intervalli MMIO noti
  for (size_t i = 0; i < MMIO_RANGE_COUNT; i++) {
    if (ranges_overlap(region->base, end_addr, mmio_ranges[i].base, mmio_ranges[i].end)) {
      klog_warn("x86_64: Regione 0x%lx-0x%lx in conflitto con MMIO 0x%lx-0x%lx", region->base, end_addr, mmio_ranges[i].base, mmio_ranges[i].end);
      return false;
    }
  }

  // Verifica se ricade in zone ACPI riservate
  for (size_t i = 0; i < ACPI_RANGE_COUNT; i++) {
    if (ranges_overlap(region->base, end_addr, acpi_reserved_ranges[i].base, acpi_reserved_ranges[i].end)) {
      klog_warn("x86_64: Regione 0x%lx-0x%lx all'interno area ACPI 0x%lx-0x%lx", region->base, end_addr, acpi_reserved_ranges[i].base, acpi_reserved_ranges[i].end);
      return false;
    }
  }

  return true;
}

/*
 * ============================================================================
 * IMPLEMENTAZIONE INTERFACCIA ARCHITETTURALE
 * ============================================================================
 */

/**
 * @brief Rileva e popola l'array delle regioni di memoria (IMPLEMENTAZIONE x86_64)
 *
 * FUNZIONAMENTO:
 * 1. Verifica che Limine abbia fornito una memory map valida
 * 2. Itera attraverso tutte le entries della memory map Limine
 * 3. Converte ogni entry da formato Limine a formato standard
 * 4. Valida ogni regione per compatibilità x86_64
 * 5. Calcola statistiche aggregate durante la scansione
 * 6. Ordina le regioni per indirizzo base (Limine dovrebbe già farlo)
 *
 * @param regions Array dove salvare le regioni (fornito dal chiamante)
 * @param max_regions Dimensione massima dell'array per evitare overflow
 * @return Numero di regioni valide rilevate, 0 se errore critico
 */
size_t arch_memory_detect_regions(memory_region_t *regions, size_t max_regions) {
  // Validazione parametri di input
  if (!regions || max_regions == 0) {
    klog_error("x86_64: Parametri non validi per detect_regions");
    return 0;
  }

  // Verifica che Limine abbia fornito la memory map
  if (!memmap_request.response) {
    klog_error("x86_64: Limine non ha fornito memory map response");
    klog_error("x86_64: Verificare che il bootloader sia Limine compatibile");
    return 0;
  }

  struct limine_memmap_response *response = memmap_request.response;

  // Validazione response di Limine
  if (response->entry_count == 0) {
    klog_error("x86_64: Memory map Limine vuota - sistema non bootabile");
    return 0;
  }

  if (response->entry_count > MAX_MEMORY_REGIONS) {
    klog_warn("x86_64: Troppe regioni memoria (%lu), possibile corruzione", response->entry_count);
    klog_warn("x86_64: Limitando a %d regioni per sicurezza", MAX_MEMORY_REGIONS);
  }

  // Reset statistiche per nuovo calcolo
  memset(&cached_stats, 0, sizeof(cached_stats));

  size_t regions_count = 0;
  u64 total_entries = response->entry_count;

  // Limita il numero di entries per sicurezza
  if (total_entries > max_regions) {
    total_entries = max_regions;
    klog_warn("x86_64: Limitando regioni da %lu a %zu", response->entry_count, max_regions);
  }

  klog_info("x86_64: Scansione %lu regioni memoria da Limine", total_entries);

  /*
   * LOOP PRINCIPALE: Conversione entries Limine → regioni standard
   */
  for (u64 i = 0; i < total_entries; i++) {
    struct limine_memmap_entry *limine_entry = response->entries[i];

    // Validazione entry Limine
    if (!limine_entry) {
      klog_warn("x86_64: Entry Limine %lu è NULL, saltando", i);
      continue;
    }

    // Conversione formato Limine → formato standard
    memory_region_t *region = &regions[regions_count];
    region->base = limine_entry->base;
    region->length = limine_entry->length;
    region->type = convert_limine_type(limine_entry->type);

    // Validazione regione per x86_64
    if (!validate_memory_region(region)) {
      klog_warn("x86_64: Regione %lu (0x%lx-0x%lx) non valida, ignorata", i, region->base, region->base + region->length - 1);
      continue;
    }

    // Debug logging dettagliato per la regione
    // klog_debug("x86_64: Regione %zu: 0x%016lx-0x%016lx (%lu MB) %s", regions_count, region->base, region->base + region->length - 1, region->length / MB,
    //           memory_type_name(region->type));

    /*
     * AGGIORNAMENTO STATISTICHE durante la scansione
     */
    cached_stats.total_memory += region->length;

    switch (region->type) {
    case MEMORY_USABLE:
    case MEMORY_BOOTLOADER_RECLAIMABLE:
    case MEMORY_ACPI_RECLAIMABLE:
      // Memoria recuperabile dal kernel
      cached_stats.usable_memory += region->length;

      // Traccia la regione usable più grande
      if (region->length > cached_stats.largest_free_region) {
        cached_stats.largest_free_region = region->length;
      }
      break;

    case MEMORY_EXECUTABLE_AND_MODULES:
      // Memoria occupata dal kernel
      cached_stats.executable_memory += region->length;
      break;

    case MEMORY_RESERVED:
    case MEMORY_ACPI_NVS:
    case MEMORY_BAD:
    case MEMORY_FRAMEBUFFER:
    default:
      // Memoria non recuperabile
      cached_stats.reserved_memory += region->length;
      break;
    }

    // Incrementa contatore regioni valide
    regions_count++;
  }

  // Marca le statistiche come valide
  stats_valid = true;

  // Log riassuntivo
  klog_info("x86_64: Rilevate %zu regioni valide su %lu totali", regions_count, response->entry_count);
  klog_info("x86_64: Memoria totale: %lu MB, utilizzabile: %lu MB", cached_stats.total_memory / MB, cached_stats.usable_memory / MB);

  // Verifica che ci sia abbastanza memoria per far girare il kernel
  if (cached_stats.usable_memory < (MIN_USABLE_MEMORY_MB * MB)) {
    klog_error("x86_64: Memoria utilizzabile insufficiente: %lu MB (minimo %d MB)", cached_stats.usable_memory / MB, MIN_USABLE_MEMORY_MB);
    klog_error("x86_64: Sistema potrebbe non essere stabile");
  }

  return regions_count;
}

/**
 * @brief Inizializzazione architetturale x86_64 per memoria
 *
 * Esegue tutti i controlli e setup specifici per x86_64 prima che
 * il sistema di memoria generico possa iniziare a funzionare.
 *
 * RESPONSABILITÀ:
 * - Verifica che Limine bootloader sia presente e configurato
 * - Controllo che l'ambiente x86_64 sia corretto
 * - Setup di eventuali registri o strutture specifiche
 * - Validazione dell'hardware di sistema
 */
void arch_memory_init(void) {
  klog_info("x86_64: Inizializzazione sottosistema memoria");

  // Verifica presenza bootloader Limine
  if (!memmap_request.response) {
    klog_panic("x86_64: Bootloader Limine richiesto ma memory map non disponibile!");
    klog_panic("x86_64: Assicurarsi di usare Limine come bootloader");
    // Il sistema non può continuare senza memory map
  }

  // Log informazioni architettura
  klog_info("x86_64: Architettura rilevata: %s", arch_get_name());
  klog_info("x86_64: Dimensione pagina: %zu bytes (%zu KB)", PAGE_SIZE, PAGE_SIZE / KB);
  klog_info("x86_64: Indirizzi fisici supportati: %d bit (max 0x%lx)", X86_64_MAX_PHYSICAL_BITS, X86_64_MAX_PHYSICAL_ADDR);

  // Verifica che la memory map Limine sia sensata
  struct limine_memmap_response *response = memmap_request.response;
  klog_info("x86_64: Memory map Limine: %lu entries", response->entry_count);

  if (response->entry_count > MAX_MEMORY_REGIONS) {
    klog_warn("x86_64: Numero elevato di regioni memoria (%lu), possibile frammentazione", response->entry_count);
  }

  // Controlli hardware specifici
  u32 eax, ebx, ecx, edx;

  cpu_cpuid(1, 0, &eax, &ebx, &ecx, &edx);

  if (!(edx & (1 << 6))) {
    klog_panic("x86_64: CPU priva di PAE, requisito indispensabile");
  } else {
    klog_debug("x86_64: PAE supportato");
  }

  if (!(edx & (1 << 12))) {
    klog_warn("x86_64: MTRR non supportato dalla CPU");
  } else {
    klog_debug("x86_64: MTRR supportato");
  }

  if (!cpu_supports_nx()) {
    klog_warn("x86_64: Bit NX non supportato - protezione esecuzione limitata");
  } else {
    klog_debug("x86_64: Bit NX supportato");
  }

  cpu_cpuid(0x80000008, 0, &eax, &ebx, &ecx, &edx);
  u32 phys_bits = eax & 0xff;
  klog_info("x86_64: CPU supporta %u bit di indirizzo fisico", phys_bits);
  if (phys_bits > X86_64_MAX_PHYSICAL_BITS) {
    klog_warn("x86_64: CPU riporta %u bit fisici oltre il limite gestito (%d)", phys_bits, X86_64_MAX_PHYSICAL_BITS);
  }

  klog_info("x86_64: Inizializzazione memoria completata con successo");
}

/**
 * @brief Verifica validità regione per architettura x86_64
 *
 * Implementa controlli specifici x86_64 per verificare che una
 * regione di memoria sia utilizzabile su questa architettura.
 *
 * @param base Indirizzo fisico base della regione
 * @param length Dimensione della regione in byte
 * @return true se regione valida per x86_64, false altrimenti
 */
bool arch_memory_region_valid(u64 base, u64 length) {
  // Controllo parametri base
  if (length == 0) {
    return false;
  }

  // Controllo overflow nella somma
  if (base + length < base) {
    return false;
  }

  // Controllo limiti fisici x86_64 (52 bit max)
  u64 end_addr = base + length - 1;
  if (end_addr > X86_64_MAX_PHYSICAL_ADDR) {
    return false;
  }

  // Protezione pagina 0 (NULL pointer protection)
  // Non permettere regioni che includono l'indirizzo 0
  if (base < X86_64_RESERVED_PAGE_0 && (base + length) > 0) {
    // La regione include l'indirizzo 0, potenzialmente pericolosa
    klog_debug("x86_64: Regione include pagina 0 (NULL protection): base=0x%lx", base);
    // Non blocchiamo, ma è un warning
  }

  // Controllo allineamento consigliato
  // x86_64 preferisce regioni allineate a boundary di pagina
  if (base % PAGE_SIZE != 0) {
    // Non fatale, ma sub-ottimale
    klog_debug("x86_64: Regione non allineata a pagina: base=0x%lx", base);
  }

  if (length % PAGE_SIZE != 0) {
    // Dimensione non multipla di pagina, potrebbero esserci sprechi
    klog_debug("x86_64: Dimensione regione non multipla di pagina: %lu bytes", length);
  }

  // Controlli aggiuntivi x86_64 specifici
  for (size_t i = 0; i < MMIO_RANGE_COUNT; i++) {
    if (ranges_overlap(base, end_addr, mmio_ranges[i].base, mmio_ranges[i].end)) {
      klog_debug("x86_64: Intervallo 0x%lx-0x%lx collide con MMIO 0x%lx-0x%lx", base, end_addr, mmio_ranges[i].base, mmio_ranges[i].end);
      return false;
    }
  }

  for (size_t i = 0; i < ACPI_RANGE_COUNT; i++) {
    if (ranges_overlap(base, end_addr, acpi_reserved_ranges[i].base, acpi_reserved_ranges[i].end)) {
      klog_debug("x86_64: Intervallo 0x%lx-0x%lx in area ACPI 0x%lx-0x%lx", base, end_addr, acpi_reserved_ranges[i].base, acpi_reserved_ranges[i].end);
      return false;
    }
  }

  // Validazione rispetto a limiti specifici del chipset

  return true;
}

/**
 * @brief Ottiene statistiche memoria calcolate per x86_64
 *
 * Ritorna le statistiche aggregate della memoria, calcolate durante
 * l'ultima chiamata a arch_memory_detect_regions().
 *
 * @param stats Puntatore alla struttura da riempire (non può essere NULL)
 *
 * @note Se detect_regions() non è mai stato chiamato, le statistiche saranno zero
 */
void arch_memory_get_stats(memory_stats_t *stats) {

  if (!stats) {
    klog_error("x86_64: Parametro stats NULL in get_stats");
    return;
  }

  if (!stats_valid) {
    klog_warn("x86_64: Statistiche non valide - detect_regions non chiamato?");
    memset(stats, 0, sizeof(memory_stats_t));
    return;
  }

  // Copia le statistiche cached
  memcpy(stats, &cached_stats, sizeof(memory_stats_t));
}

/**
 * @brief Ritorna identificativo architettura x86_64
 *
 * @return Stringa costante "x86_64" per identificazione
 */
const char *arch_get_name(void) {
  return "x86_64";
}

/*
 * ============================================================================
 * NOTE IMPLEMENTATIVE
 * ============================================================================
 *
 * FILOSOFIA DI DESIGN:
 * Questo file implementa SOLO l'interfaccia hardware specifica per x86_64.
 * Non contiene logica di presentazione o reporting generale - quella
 * responsabilità appartiene al layer arch-agnostic in mm/memory.c
 *
 * SEPARAZIONE DELLE RESPONSABILITÀ:
 * - arch/x86_64/memory.c: Hardware interface (questo file)
 * - mm/memory.c: Kernel logic e user interface
 * - mm/pmm.c: Page management arch-independent
 *
 * Questo design mantiene il codice modulare e permette il supporto
 * di multiple architetture senza duplicazione di logica.
 */