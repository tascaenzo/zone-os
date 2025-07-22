#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <klib/klog.h>
#include <lib/stdio.h>
#include <mm/memory.h>
#include <mm/pmm.h>

// Richiesta framebuffer a Limine
volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

/**
 * @brief Test essenziale del PMM
 */
static void test_pmm_simple(void) {
  klog_info("=== TEST PMM ===");

  // Test allocazione singola pagina
  void *page = pmm_alloc_pages(8);
  if (page) {
    klog_info("Allocazione riuscita: 0x%lx", (u64)page);

    // Test liberazione
    pmm_result_t result = pmm_free_pages(page, 80);
    if (result == PMM_SUCCESS) {
      klog_info("Liberazione riuscita");
    } else {
      klog_error("Errore liberazione: %d", result);
    }
  } else {
    klog_error("Allocazione fallita");
  }

  // Statistiche finali
  const pmm_stats_t *stats = pmm_get_stats();
  if (stats) {
    klog_info("Statistiche: %lu libere, %lu allocazioni", stats->free_pages, stats->alloc_count);
  }
}

void kmain(void) {
  // Inizializzazione base
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
  console_init();
  console_clear();

  klog_info("=== KERNEL AVVIATO ===");

  // Inizializzazione memoria
  klog_info("Inizializzazione memoria...");
  memory_init();

  klog_info("Inizializzazione PMM...");
  pmm_result_t result = pmm_init();
  if (result != PMM_SUCCESS) {
    klog_panic("PMM init fallito: %d", result);
  }

  klog_info("PMM inizializzato con successo");
  pmm_print_info();

  // Test
  test_pmm_simple();

  klog_info("Test completati - sistema in idle");
  while (1) {
    __asm__ volatile("hlt");
  }
}