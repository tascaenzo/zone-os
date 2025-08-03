#include <arch/cpu.h>
#include <arch/memory.h>
#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <interrupts.h>
#include <klib/klog.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <mm/heap/heap.h>
#include <mm/memory.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

// Basic INT3 test handler
static void handle_breakpoint(arch_interrupt_context_t *ctx, u8 vec) {
  klog_info("INT3 handler called — RIP = %p", ctx->rip);
}

void kmain(void) {
  // 1. Video
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
  console_init();
  console_clear();

  klog_info("=== ZONE-OS MICROKERNEL ===");
  klog_info("Booted via Limine, architecture: %s", arch_get_name());

  // 2. PMM init
  memory_init();
  if (pmm_init() != PMM_SUCCESS)
    klog_panic("PMM init failed");

  const pmm_stats_t *pmm = pmm_get_stats();
  klog_info("PMM: %lu MB free", pmm->free_pages * PAGE_SIZE / (1024 * 1024));

  // 3. VMM
  vmm_init();
  klog_info("VMM initialized");

  // 4. IDT & interrupt init
  interrupts_init();
  interrupts_register_handler(3, handle_breakpoint);
  klog_info("IDT and ISR stubs initialized");

  // 5. Debug: dump IDT[3] address and stub ptr
  extern void *isr_stub_table[];
  struct {
    u16 limit;
    u64 base;
  } __attribute__((packed)) idtr = {0};

  asm volatile("sidt %0" : "=m"(idtr));
  u64 *idt_base = (u64 *)idtr.base;
  u64 isr_addr = (u64)isr_stub_table[3];
  klog_info("Stub[3] = %p", (void *)isr_addr);

  // 6. Verifica RFLAGS.IF prima e dopo STI
  u64 flags;
  asm volatile("pushfq; popq %0" : "=r"(flags));
  klog_info("Before sti: IF=%d", (flags >> 9) & 1);
  interrupts_enable();
  asm volatile("pushfq; popq %0" : "=r"(flags));
  klog_info("After sti: IF=%d", (flags >> 9) & 1);

  // 7. INT3 call test
  klog_info("Trigger INT3...");
  asm volatile("int3");
  klog_info("Returned from INT3");

  // 8. Heap test
  heap_init();
  memory_late_init();

  // 9. Final memory info
  const pmm_stats_t *final = pmm_get_stats();
  klog_info("Memory: %lu MB free, %lu MB used", final->free_pages * PAGE_SIZE / (1024 * 1024), final->used_pages * PAGE_SIZE / (1024 * 1024));

  // 10. Idle loop
  klog_info("ZONE-OS READY — entering idle");
  while (1)
    asm volatile("hlt");
}
