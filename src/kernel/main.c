#include <arch/x86_64/gdt/gdt.h>
#include <arch/x86_64/memory/memory.h>
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

// === Richiesta framebuffer (LIMINE) ===
volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

void kmain(void) {
  // === Inizializzazione grafica ===
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
  console_init();
  console_clear();

  // === Inizializzazione GDT + TSS ===
  // In questo punto carichiamo la GDT con i segmenti per Ring 0 e Ring 3
  // - Ring 0 (attivo): 0x08 = codice kernel, 0x10 = dati kernel
  // - Ring 3 (pronto): 0x28 = codice user, 0x20 = dati user
  // Carichiamo anche il TSS per supporto a IST/interrupt e future syscall
  gdt_init();
  klog_info("GDT + TSS initialized (Ring 0 attivo, Ring 3 pronto)");

  // === Log iniziale ===
  klog_info("=== ZONE-OS MICROKERNEL ===");
  klog_info("Booted via Limine, architecture: %s", arch_get_name());

  // === Inizializzazione memoria fisica (PMM) ===
  memory_init();
  if (pmm_init() != PMM_SUCCESS)
    klog_panic("PMM init failed");

  const pmm_stats_t *pmm = pmm_get_stats();
  klog_info("PMM: %lu MB free", pmm->free_pages * PAGE_SIZE / (1024 * 1024));

  // === Inizializzazione memoria virtuale (VMM) ===
  vmm_init();
  klog_info("VMM initialized");

  // === Inizializzazione heap e memoria ritardata ===
  heap_init();
  memory_late_init();

  // === Statistiche finali memoria ===
  const pmm_stats_t *final = pmm_get_stats();
  klog_info("Memory: %lu MB free, %lu MB used", final->free_pages * PAGE_SIZE / (1024 * 1024), final->used_pages * PAGE_SIZE / (1024 * 1024));

  // === Test interruzione software (INT3) ===
  klog_info("ZONE-OS READY — entering idle");

  klog_info("Trigger INT3...");
  asm volatile("int3");
  klog_info("Returned from INT3");

  // === Loop di idle ===
  while (1)
    asm volatile("hlt");
}
