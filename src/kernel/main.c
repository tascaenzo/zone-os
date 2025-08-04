#include <arch/cpu.h>
#include <arch/memory.h>
#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <interrupts/exceptions.h>
#include <interrupts/interrupts.h>
#include <klib/klog.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <mm/heap/heap.h>
#include <mm/memory.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

// Richiesta framebuffer (LIMINE)
volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

void kmain(void) {
  // 1. Inizializzazione video
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
  console_init();
  console_clear();

  klog_info("=== ZONE-OS MICROKERNEL ===");
  klog_info("Booted via Limine, architecture: %s", arch_get_name());

  // 2. Init memoria fisica (PMM)
  memory_init();
  if (pmm_init() != PMM_SUCCESS)
    klog_panic("PMM init failed");

  const pmm_stats_t *pmm = pmm_get_stats();
  klog_info("PMM: %lu MB free", pmm->free_pages * PAGE_SIZE / (1024 * 1024));

  // 3. Init memoria virtuale (VMM)
  vmm_init();
  klog_info("VMM initialized");

  // 4. Init IDT + gestori interrupt
  interrupts_init();
  exceptions_init();
  klog_info("IDT and CPU exception handlers initialized");

  // 5. Verifica RFLAGS.IF prima/dopo STI
  u64 flags;
  asm volatile("pushfq; popq %0" : "=r"(flags));
  klog_info("Before sti: IF=%d", (flags >> 9) & 1);
  interrupts_enable();
  asm volatile("pushfq; popq %0" : "=r"(flags));
  klog_info("After sti: IF=%d", (flags >> 9) & 1);

  // 6. Trigger INT3 per test gestore breakpoint

  // 7. Init heap e late memory
  heap_init();
  memory_late_init();

  // 8. Statistiche finali memoria
  const pmm_stats_t *final = pmm_get_stats();
  klog_info("Memory: %lu MB free, %lu MB used", final->free_pages * PAGE_SIZE / (1024 * 1024), final->used_pages * PAGE_SIZE / (1024 * 1024));

  // 9. Idle loop
  klog_info("ZONE-OS READY — entering idle");

  klog_info("Trigger INT3...");
  asm volatile("int3");
  klog_info("Returned from INT3");

  int zero = 0;
  int one = 1;
  klog_info("Zero division test: %d / %d = %d", one, zero, one / zero); // Questo causerà un errore di divisione per zero
  klog_info("Zero division test returned successfully, this is unexpected");

  while (1)
    asm volatile("hlt");
}
