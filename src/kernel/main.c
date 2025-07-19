#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <lib/stdio.h> // ← Aggiunto per kprintf

// Richiesta framebuffer a Limine (OBBLIGATORIA)
volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

void kmain(void) {
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
  console_init();
  console_clear();

  // === Test kprintf ===
  kprintf("=== ZONE-OS Kernel ===\n");
  kprintf("Framebuffer console ready!\n\n");

  // Test basic types
  kprintf("Integer tests:\n");
  kprintf("  Decimal: %d\n", 42);
  kprintf("  Negative: %d\n", -123);
  kprintf("  Unsigned: %u\n", 3000000000U);
  kprintf("  Hex lower: 0x%x\n", 0xDEADBEEF);
  kprintf("  Hex upper: 0x%X\n", 0xCAFEBABE);
  kprintf("\n");

  // Test character and string
  kprintf("Character tests:\n");
  kprintf("  Single char: %c\n", 'A');
  kprintf("  String: %s\n", "Hello, ZONE-OS!");
  kprintf("  Null string: %s\n", (char *)0);
  kprintf("\n");

  // Test pointer
  kprintf("Pointer tests:\n");
  kprintf("  Function pointer: %p\n", (void *)kmain);
  kprintf("  Framebuffer addr: %p\n", fb->address);
  kprintf("  Stack variable: %p\n", (void *)&fb);
  kprintf("\n");

  // Test special characters
  kprintf("Special tests:\n");
  kprintf("  Percent literal: 100%% complete\n");
  kprintf("  Unknown spec: %z (should show %%z)\n");
  kprintf("\n");

  // Test console functions
  kprintf("Testing other I/O functions:\n");
  kputs("  This is from kputs()");
  kprintf("  Single char: ");
  kputchar('X');
  kputchar('\n');
  kprintf("\n");

  // System info
  kprintf("System Information:\n");
  kprintf("  Framebuffer: %ux%u @ %u bpp\n", (unsigned)fb->width, (unsigned)fb->height, (unsigned)fb->bpp);
  kprintf("  Memory pitch: %u bytes\n", (unsigned)fb->pitch);
  kprintf("\n");

  // Boot complete
  kprintf("Boot sequence complete!\n");
  kprintf("Kernel is running and ready.\n");

  // Original alphabet test
  kprintf("\nAlphabet test:\n");
  for (char c = 'A'; c <= 'Z'; c++) {
    kputchar(c);
  }
  kputchar('\n');

  for (char c = 'a'; c <= 'z'; c++) {
    kputchar(c);
  }
  kputchar('\n');

  kprintf("\nKernel halted. System ready.\n");

  while (1) {
    __asm__ volatile("hlt");
  }
}