#include <klib/bitmap.h>
#include <klib/klog.h>
#include <klib/list.h>
#include <lib/math.h>
#include <lib/string.h>
#include <mm/heap/buddy.h>

/*
 * Implementazione "didattica" di un allocatore Buddy.
 * Ogni blocco libero inizia con un header (buddy_block_header_t)
 * contenente l'ordine e un magic number per verificare integrità.
 * Le funzioni di allocazione e liberazione operano sempre in potenze
 * di 2 e sfruttano liste separate per ogni ordine possibile.
 * Tutte le operazioni pubbliche sono protette da uno spinlock,
 * rendendo l'allocatore sicuro in contesti concorrenti.
 */

/*
 * Calcola l'ordine (potenza di due) necessario per soddisfare una
 * richiesta di 'size' byte.  Viene considerato anche lo spazio
 * richiesto per l'header del blocco.  Il risultato è sempre
 * compreso tra BUDDY_MIN_ORDER e BUDDY_MAX_ORDER.
 */
static u8 order_for_request(size_t size) {
  size_t needed = size + sizeof(buddy_block_header_t);
  if (needed < BUDDY_MIN_BLOCK_SIZE)
    needed = BUDDY_MIN_BLOCK_SIZE;

  u8 order = BUDDY_MIN_ORDER;
  size_t block = BUDDY_MIN_BLOCK_SIZE;
  while (block < needed && order < BUDDY_MAX_ORDER) {
    block <<= 1;
    order++;
  }
  if (order > BUDDY_MAX_ORDER)
    order = BUDDY_MAX_ORDER;
  return order;
}

/* Converte un indirizzo assoluto nell'indice corrispondente nella bitmap */
static size_t index_for_addr(buddy_allocator_t *a, u64 addr) {
  return (addr - a->base_addr) / BUDDY_MIN_BLOCK_SIZE;
}

/*
 * Inserisce un blocco nella free list dell'ordine specificato.
 * L'header viene inizializzato con magic di blocco libero.
 */
static void insert_block(buddy_allocator_t *a, u64 addr, u8 order) {
  buddy_block_t *block = (buddy_block_t *)addr;
  block->header.order = order;
  block->header.magic = BUDDY_FREE_MAGIC;
  list_insert_after(&a->free_lists[order], &block->node);
}

/* Rimuove un blocco da una free list senza altre verifiche */
static void remove_block(buddy_block_t *block) {
  list_remove(&block->node);
}

/* Cerca un blocco specifico all'interno di una free list */
static buddy_block_t *find_block(buddy_allocator_t *a, u64 addr, u8 order) {
  list_node_t *it;
  LIST_FOR_EACH(it, &a->free_lists[order]) {
    buddy_block_t *b = LIST_ENTRY(it, buddy_block_t, node);
    if ((u64)b == addr)
      return b;
  }
  return NULL;
}

/*
 * Inizializza l'allocatore predisponendo le free list e il bitmap.
 * La regione passata viene spezzata in blocchi della massima
 * dimensione possibile e inserita nelle liste.
 */
bool buddy_init(buddy_allocator_t *a, u64 base_addr, u64 size_in_bytes, u64 *bitmap_storage, size_t bitmap_bits) {
  a->base_addr = base_addr & ~(BUDDY_MIN_BLOCK_SIZE - 1);
  a->total_size = size_in_bytes & ~(BUDDY_MIN_BLOCK_SIZE - 1);

  for (int i = 0; i <= BUDDY_MAX_ORDER; i++)
    list_init(&a->free_lists[i]);

  size_t needed_bits = a->total_size / BUDDY_MIN_BLOCK_SIZE;
  if (bitmap_bits < needed_bits) {
    klog_warn("buddy: bitmap too small (%zu < %zu)", bitmap_bits, needed_bits);
    return false;
  }

  bitmap_init(&a->allocation_map, bitmap_storage, bitmap_bits);
  bitmap_clear_all(&a->allocation_map);
  spinlock_init(&a->lock);

  u64 addr = a->base_addr;
  u64 remaining = a->total_size;
  while (remaining >= BUDDY_MIN_BLOCK_SIZE) {
    u8 order = BUDDY_MAX_ORDER;
    while (order > BUDDY_MIN_ORDER) {
      u64 bs = 1UL << order;
      if (bs <= remaining && (addr % bs) == 0)
        break;
      order--;
    }
    u64 block_size = 1UL << order;
    insert_block(a, addr, order);
    addr += block_size;
    remaining -= block_size;
  }

  a->total_allocs = 0;
  a->total_frees = 0;
  a->failed_allocs = 0;
  return true;
}

/*
 * Alloca un blocco di almeno 'size' byte.
 * La ricerca avviene dalla lista dell'ordine minimo necessario
 * aumentando fino a trovare un blocco disponibile. Eventuali blocchi
 * più grandi vengono divisi ricorsivamente.
 */
u64 buddy_alloc(buddy_allocator_t *a, size_t size) {
  if (size == 0)
    return 0;

  spinlock_lock(&a->lock);
  u8 order = order_for_request(size);
  int current = order;
  /* Cerca il primo ordine con un blocco disponibile */
  while (current <= BUDDY_MAX_ORDER && list_is_empty(&a->free_lists[current]))
    current++;
  if (current > BUDDY_MAX_ORDER) {
    a->failed_allocs++;
    spinlock_unlock(&a->lock);
    klog_warn("buddy: alloc failed for %zu bytes", size);
    return 0;
  }

  list_node_t *node = a->free_lists[current].next;
  buddy_block_t *block = LIST_ENTRY(node, buddy_block_t, node);
  if (block->header.magic != BUDDY_FREE_MAGIC)
    klog_warn("buddy: allocating block without free magic!");
  remove_block(block);

  /* Suddivide il blocco più grande in blocchi più piccoli fino
   * a raggiungere l'ordine desiderato */
  while (current > order) {
    current--;
    u64 split_size = 1UL << current;
    u64 buddy_addr = (u64)block + split_size;
    insert_block(a, buddy_addr, current);
  }

  /* Aggiorna header e bitmap per segnare il blocco come allocato */
  block->header.order = order;
  block->header.magic = BUDDY_ALLOC_MAGIC;
  size_t start = index_for_addr(a, (u64)block);
  size_t count = (1UL << order) / BUDDY_MIN_BLOCK_SIZE;
  for (size_t i = 0; i < count; i++)
    bitmap_set(&a->allocation_map, start + i);

  a->total_allocs++;
  u64 result = (u64)block + sizeof(buddy_block_header_t);
  spinlock_unlock(&a->lock);
  return result;
}

/*
 * Libera il blocco precedentemente restituito da buddy_alloc.
 * L'ordine viene letto dall'header e, se possibile, il blocco
 * viene fuso con il suo buddy per ridurre la frammentazione.
 */
void buddy_free(buddy_allocator_t *a, u64 addr) {
  u64 block_addr = addr - sizeof(buddy_block_header_t);
  if (block_addr < a->base_addr || block_addr >= a->base_addr + a->total_size)
    return;
  spinlock_lock(&a->lock);
  buddy_block_t *block = (buddy_block_t *)block_addr;
  if (block->header.magic != BUDDY_ALLOC_MAGIC) {
    spinlock_unlock(&a->lock);
    klog_warn("buddy: double free or invalid addr %p", (void *)addr);
    return;
  }
  u8 order = block->header.order;
  u64 block_size = 1UL << order;

  /* Verifica che tutte le entry di bitmap siano effettivamente occupate */
  size_t start = index_for_addr(a, block_addr);
  size_t count = block_size / BUDDY_MIN_BLOCK_SIZE;
  for (size_t i = 0; i < count; i++) {
    if (!bitmap_get(&a->allocation_map, start + i)) {
      spinlock_unlock(&a->lock);
      klog_warn("buddy: double free or invalid addr %p", (void *)addr);
      return;
    }
  }
  for (size_t i = 0; i < count; i++)
    bitmap_clear(&a->allocation_map, start + i);

  /* Tenta di fondere il blocco con il buddy contiguo finché
   * entrambi risultano liberi e di uguale ordine */
  while (order < BUDDY_MAX_ORDER) {
    u64 offset = block_addr - a->base_addr;
    u64 buddy_offset = offset ^ (1UL << order);
    u64 buddy_addr = a->base_addr + buddy_offset;

    /* Controlla se il buddy corrispondente è completamente libero */
    bool buddy_free_flag = true;
    size_t buddy_start = index_for_addr(a, buddy_addr);
    for (size_t i = 0; i < (1UL << order) / BUDDY_MIN_BLOCK_SIZE; i++) {
      if (bitmap_get(&a->allocation_map, buddy_start + i)) {
        buddy_free_flag = false;
        break;
      }
    }
    /* Se il buddy non è libero interrompe la fusione */
    if (!buddy_free_flag)
      break;
    /* Il buddy deve trovarsi nella free list e avere il magic corretto */
    buddy_block_t *buddy = find_block(a, buddy_addr, order);
    if (!buddy || buddy->header.magic != BUDDY_FREE_MAGIC)
      break;
    /* Rimuove il buddy dalla free list e unisce i due blocchi */
    remove_block(buddy);
    /* Aggiorna indirizzo e ordine del blocco fuso */
    block_addr = (buddy_addr < block_addr) ? buddy_addr : block_addr;
    order++;
    block_size <<= 1;
  }

  insert_block(a, block_addr, order);
  a->total_frees++;
  spinlock_unlock(&a->lock);
}

/*
 * Stampa informazioni sulle liste libere, utile per il debug.
 */
void buddy_dump(buddy_allocator_t *a) {
  for (int order = BUDDY_MIN_ORDER; order <= BUDDY_MAX_ORDER; order++) {
    size_t count = 0;
    list_node_t *it;
    LIST_FOR_EACH(it, &a->free_lists[order])
    count++;
    u64 size = 1UL << order;
    klog_info("buddy order %d (%llu bytes): %zu blocks", order, size, count);
  }
}

/*
 * Effettua una serie di controlli sulle free list per assicurare
 * che non vi siano corruzioni: ogni blocco deve trovarsi nella
 * lista dell'ordine corretto e risultare allineato.
 */
bool buddy_check_integrity(buddy_allocator_t *a) {
  for (int order = BUDDY_MIN_ORDER; order <= BUDDY_MAX_ORDER; order++) {
    list_node_t *it;
    LIST_FOR_EACH(it, &a->free_lists[order]) {
      buddy_block_t *b = LIST_ENTRY(it, buddy_block_t, node);
      if (b->header.order != order)
        return false;
      u64 addr = (u64)b;
      if (((addr - a->base_addr) % (1UL << order)) != 0)
        return false;
    }
  }
  return true;
}

/*
 * Raccoglie semplici statistiche utili a monitorare lo stato
 * dell'allocatore: memoria libera totale, blocco più grande
 * disponibile e stima di frammentazione.
 */
void buddy_get_stats(buddy_allocator_t *a, u64 *total_free, u64 *largest_free, u32 *fragmentation) {
  u64 free_mem = 0;
  u64 largest = 0;
  for (int order = BUDDY_MIN_ORDER; order <= BUDDY_MAX_ORDER; order++) {
    list_node_t *it;
    LIST_FOR_EACH(it, &a->free_lists[order]) {
      free_mem += (1UL << order);
    }
    if (!list_is_empty(&a->free_lists[order])) {
      u64 size = 1UL << order;
      if (size > largest)
        largest = size;
    }
  }
  if (total_free)
    *total_free = free_mem;
  if (largest_free)
    *largest_free = largest;
  if (fragmentation)
    *fragmentation = free_mem ? (u32)(100 - (largest * 100 / free_mem)) : 0;
}
