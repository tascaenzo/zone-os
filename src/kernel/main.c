/**
 * @file kernel/main.c
 * @brief Punto di ingresso del kernel ZONE-OS
 *
 * Inizializza sottosistemi di base: video/console, arch (GDT/IDT/IRQ, timer),
 * memoria fisica (PMM), memoria virtuale (VMM), heap e memoria “late”.
 * Esegue un breve self-test (INT3) e poi entra nel loop di idle (HLT).
 *
 * Ordine consigliato:
 *  1) Framebuffer + console (per log)
 *  2) arch_init() (GDT/IDT/IRQ/timer base)
 *  3) memory_init() → pmm_init()
 *  4) vmm_init()
 *  5) heap_init() → memory_late_init()
 *  6) Abilitazione IRQ → eventuali test → idle loop
 *
 * @author
 * @date 2025
 */

#include <arch/cpu.h>
#include <arch/platform.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <klib/klog/klog.h>
#include <lib/stdio/stdio.h>
#include <lib/string/string.h>
#include <limine.h>
#include <mm/heap/heap.h>
#include <mm/memory.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

/* === Richiesta framebuffer (LIMINE) ====================================== */
volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

void kmain(void) {
  /* --- Framebuffer + Console -------------------------------------------- */
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);

  console_init();
  console_clear();

  klog_info("Framebuffer initialized:");

  /* --- Init architettura (GDT/IDT/IRQ/timer) ----------------------------- */
  arch_init();
  klog_info("Architecture init: %s", arch_get_name());

  /* --- Banner ------------------------------------------------------------ */
  klog_info("=== ZONE-OS MICROKERNEL ===");
  klog_info("Booted via Limine");

  /* --- Memoria fisica (PMM) --------------------------------------------- */
  // memory_init(); /* early platform memory hooks */
  // if (pmm_init() != PMM_SUCCESS) {
  //   klog_panic("PMM init failed");
  // }

  // const pmm_stats_t *pmm = pmm_get_stats();
  // const size_t page_size = (size_t)arch_memory_page_size();
  // unsigned long long pmm_free_mb = (unsigned long long)(pmm->free_pages) * page_size / (1024ull * 1024ull);
  // klog_info("PMM: %llu MB free", pmm_free_mb);

  ///* --- Memoria virtuale (VMM) ------------------------------------------- */
  // vmm_init();
  // klog_info("VMM initialized");

  ///* --- Heap + init memoria tardiva -------------------------------------- */
  // heap_init();
  // memory_late_init();

  // const pmm_stats_t *final = pmm_get_stats();
  // unsigned long long mem_free_mb = (unsigned long long)(final->free_pages) * page_size / (1024ull * 1024ull);
  // unsigned long long mem_used_mb = (unsigned long long)(final->used_pages) * page_size / (1024ull * 1024ull);
  // klog_info("Memory: %llu MB free, %llu MB used", mem_free_mb, mem_used_mb);

  ///* --- Abilita IRQ e piccolo self-test (INT3) --------------------------- */
  // arch_cpu_enable_interrupts();
  // klog_info("Trigger INT3...");
  //__asm__ volatile("int3");
  // klog_info("Returned from INT3");

  /* --- Idle loop --------------------------------------------------------- */
  klog_info("ZONE-OS READY — entering idle");
  while (1) {
    arch_cpu_halt();
  }
}
