#include <arch/memory.h>
#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <klib/klog.h>
#include <lib/stdio.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/kernel_layout.h>

/*
 * ============================================================================
 * BOOTLOADER REQUESTS
 * ============================================================================
 */

volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

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
  pmm_result_t pmm_result = pmm_init();
  if (pmm_result != PMM_SUCCESS) {
    klog_panic("PMM init failed (code: %d)", pmm_result);
  }

  const pmm_stats_t *pmm_stats = pmm_get_stats();
  if (pmm_stats) {
    u64 free_mb = pmm_stats->free_pages * PAGE_SIZE / (1024 * 1024);
    klog_info("PMM initialized - Memory available: %lu MB", free_mb);
  }

  klog_info("Initializing Virtual Memory Manager...");
  vmm_init();
  klog_info("Kernel layout validation:");
  klog_info("  KERNEL_TEXT_BASE:   0x%016lx", KERNEL_TEXT_BASE);
  klog_info("  DIRECT_MAP_BASE:    0x%016lx", DIRECT_MAP_BASE);
  klog_info("  KERNEL_HEAP_BASE:   0x%016lx", KERNEL_HEAP_BASE);
  klog_info("  VMALLOC_BASE:       0x%016lx", VMALLOC_BASE);

  if (!vmm_check_integrity(vmm_kernel_space())) {
    klog_panic("VMM integrity check failed after direct mapping setup");
  }

  klog_info("Testing direct mapping...");
  void *test_page = pmm_alloc_page();
  if (test_page) {
    void *virt_page = (void *)PHYS_TO_VIRT((u64)test_page);
    *(u32 *)virt_page = 0xDEADBEEF;
    if (*(u32 *)virt_page == 0xDEADBEEF) {
      klog_info("\xE2\x9C\x93 Direct mapping test passed");
    } else {
      klog_error("\xE2\x9C\x97 Direct mapping test failed");
    }
    pmm_free_page(test_page);
  }

  /*
   * FASE 3: INIZIALIZZAZIONE VIRTUAL MEMORY MANAGER
   */
  klog_info("Initializing Virtual Memory Manager...");
  vmm_init();
  klog_info("VMM initialized successfully");

  /*
   * FASE 4: STATISTICHE FINALI
   */
  const pmm_stats_t *final_stats = pmm_get_stats();
  if (final_stats) {
    klog_info("Final memory stats: %lu MB free, %lu MB used", final_stats->free_pages * PAGE_SIZE / (1024 * 1024), final_stats->used_pages * PAGE_SIZE / (1024 * 1024));
  }

  /*
   * FASE 6: MICROKERNEL READY
   */
  klog_info("=== MICROKERNEL INITIALIZATION COMPLETE ===");
  klog_info("All tests completed - ZONE-OS microkernel ready");

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
 * │  PMM + VMM + IPC + Minimal Scheduler + System Calls         │
 * └─────────────────────────────────────────────────────────────┘
 *
 */