#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <klib/klog.h>
#include <lib/stdio.h>
#include <mm/pmm.h>

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
   * FASE 2: PHYSICAL MEMORY MANAGER (ESSENZIALE)
   *
   * Anche nei microkernel, il PMM rimane nel kernel space
   * perché è fondamentale per qualsiasi allocazione.
   */
  klog_info("Initializing Physical Memory Manager...");
  pmm_result_t pmm_result = pmm_init();
  if (pmm_result != PMM_SUCCESS) {
    klog_panic("Critical: PMM initialization failed (code: %d)", pmm_result);
  }
  klog_info("✓ Physical Memory Manager initialized");

  const pmm_stats_t *stats = pmm_get_stats();
  if (stats) {
    u64 free_mb = stats->free_pages * PAGE_SIZE / (1024 * 1024);
    klog_info("Memory available: %lu MB", free_mb);
  }

  klog_info("=== MICROKERNEL INITIALIZATION COMPLETE ===");
  klog_info("ZONE-OS microkernel ready");

  while (1) {
    /*
     * TODO: Quando implementerai IPC, questo diventerà:
     *
     * while (1) {
     *     handle_ipc_messages();    // Gestisci messaggi tra processi
     *     schedule_next_process();  // Context switch minimale
     *     __asm__ volatile("hlt");  // Attendi interrupt
     * }
     */
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