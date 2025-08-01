#include <klib/panic.h>
#include <drivers/video/console.h>
#include <klib/klog.h>
#include <arch/cpu.h>
#include <lib/stdio.h>

void panic(const char *message, isr_frame_t *frame) {
  console_clear();
  console_set_color(CONSOLE_COLOR_WHITE, CONSOLE_COLOR_RED);
  kprintf("!!! KERNEL PANIC !!!\n");
  kprintf("Reason: %s\n", message ? message : "Unknown");
  if (frame) {
    kprintf("Instruction: RIP=0x%lx\n", frame->rip);
    kprintf("Error Code: 0x%lx\n", frame->err_code);
  }
  kprintf("Halting...\n");
  while (1) {
    cpu_disable_interrupts();
    cpu_halt();
  }
}
