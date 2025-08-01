#include <arch/memory.h>
#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <klib/klog.h>
#include <arch/interrupts.h>
#include <arch/cpu.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <mm/heap/heap.h>
#include <mm/memory.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

/*
 * ============================================================================
 * BOOTLOADER REQUESTS
 * ============================================================================
 */

volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

void heap_self_test(void) {
  klog_info("heap_self_test: inizio test allocatore heap...");
  void *ptrs[64];

  for (int i = 0; i < 64; ++i)
    ptrs[i] = kmalloc(32 + (i % 5) * 16);

  for (int i = 0; i < 64; ++i)
    kfree(ptrs[i]);

  if (!heap_check_integrity())
    klog_error("heap_self_test: ERRORE di integrità heap!");
  else
    klog_info("heap_self_test: OK - allocazione e liberazione funzionano.");

  klog_info("heap_self_test: test su allocazioni grandi (buddy)...");

  void *big1 = kmalloc(64 * 1024);       // 64 KB
  void *big2 = kmalloc(512 * 1024);      // 512 KB
  void *big3 = kmalloc(2 * 1024 * 1024); // 2 MB

  kfree(big1);
  kfree(big2);
  kfree(big3);

  klog_info("heap_self_test: allocazioni grandi liberate correttamente");
}

/*
 * ============================================================================
 * MICROKERNEL ENTRY POINT
 * ============================================================================
 */

void kmain(void) {
  /*
   * FASE 1: INIZIALIZZAZIONE VIDEO E CONSOLE
   */
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
  console_init();
  console_clear();

  klog_info("=== ZONE-OS MICROKERNEL ===");
  klog_info("Architecture: %s", arch_get_name());
  klog_info("Microkernel initializing...");

  /*
   * FASE 2: INIZIALIZZAZIONE PHYSICAL MEMORY MANAGER
   */
  klog_info("Initializing Physical Memory Manager...");
  memory_init();
  pmm_result_t pmm_result = pmm_init();
  if (pmm_result != PMM_SUCCESS) {
    klog_panic("PMM init failed (code: %d)", pmm_result);
  }

  const pmm_stats_t *pmm_stats = pmm_get_stats();
  if (pmm_stats) {
    u64 free_mb = pmm_stats->free_pages * PAGE_SIZE / (1024 * 1024);
    klog_info("PMM initialized - Memory available: %lu MB", free_mb);
  }

  /*
   * FASE 3: INIZIALIZZAZIONE VIRTUAL MEMORY MANAGER
   */
  klog_info("Initializing Virtual Memory Manager...");
  vmm_init();
  klog_info("VMM initialized successfully");

  /*
   * FASE 4: INIZIALIZZAZIONE GESTIONE INTERRUPT
   */
  klog_info("Initializing IDT and PIC...");
  idt_init();
  pic_remap(0x20, 0x28);
  cpu_enable_interrupts();
  klog_info("Interrupts enabled");

  /*
   * FASE 5: TEST HEAP COMPLETI
   */
  heap_init();
  memory_late_init();

  /*
   * FASE 6: STATISTICHE FINALI
   */
  const pmm_stats_t *final_stats = pmm_get_stats();
  if (final_stats) {
    klog_info("Final memory stats: %lu MB free, %lu MB used", final_stats->free_pages * PAGE_SIZE / (1024 * 1024), final_stats->used_pages * PAGE_SIZE / (1024 * 1024));
  }

  /*
   * FASE 7: MICROKERNEL READY
   */
  klog_info("=== MICROKERNEL INITIALIZATION COMPLETE ===");
  klog_info("All tests completed - ZONE-OS microkernel ready");

  klog_info("Running comprehensive heap tests...");
  heap_self_test(); // ✅ CHIAMATA AGGIUNTA QUI
  while (1) {
    __asm__ volatile("hlt");
  }
}

/*
 * ============================================================================
 * MICROKERNEL
 * ============================================================================
 *
 * MICROKERNEL (ZONE-OS):
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    USER SPACE                               │
 * ├─────────────┬─────────────┬───────────────┬─────────────────┤
 * │File System  │Device Mgr   │Network Stack  │Application      │
 * │Server       │Server       │Server         │Processes        │
 * └─────────────┴─────────────┴───────────────┴─────────────────┘
 *               ↕ IPC Messages ↕
 * ┌─────────────────────────────────────────────────────────────┐
 * │                MICROKERNEL                                  │
 * │  PMM + VMM + HEAP + IPC + Minimal Scheduler + System Calls  │
 * └─────────────────────────────────────────────────────────────┘
 *
 */