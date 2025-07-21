#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <klib/klog.h> // ← Aggiunto per klog
#include <lib/stdio.h>

// Richiesta framebuffer a Limine (OBBLIGATORIA)
volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

void kmain(void) {
  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

  framebuffer_init(fb->address, fb->width, fb->height, fb->pitch, fb->bpp);
  console_init();
  console_clear();

  // === Test klog system ===
  klog_info("ZONE-OS Kernel starting...");
  klog_info("Framebuffer initialized: %ux%u @ %u bpp", (unsigned)fb->width, (unsigned)fb->height, (unsigned)fb->bpp);

  // Simuliamo il boot process con klog
  klog_debug("Initializing kernel subsystems...");
  klog_info("Console driver loaded successfully");
  klog_info("Framebuffer driver loaded successfully");

  // Test diversi livelli
  klog_debug("This is a debug message (dettagli interni)");
  klog_info("System boot progress: %d%% complete", 25);
  klog_warn("Low memory warning: only %dMB available", 64);
  klog_error("Failed to load optional module: %s", "network_driver");

  kprintf("\n=== Switching to different log levels ===\n");

  // Test filtraggio livelli
  klog_info("Current log level: %d", klog_get_level());

  klog_info("Setting log level to WARN - debug and info will be filtered");
  klog_set_level(KLOG_LEVEL_WARN);

  klog_debug("This debug message should NOT appear");
  klog_info("This info message should NOT appear");
  klog_warn("This warning SHOULD appear");
  klog_error("This error SHOULD appear");

  // Reset a livello normale
  klog_set_level(KLOG_LEVEL_INFO);
  klog_info("Log level reset to INFO");

  kprintf("\n=== Testing KLOG_ASSERT macro ===\n");

  // Test assert che passa
  int test_value = 42;
  KLOG_ASSERT(test_value == 42, "Test value should be 42, got %d", test_value);
  klog_info("KLOG_ASSERT test passed!");

  // Test assert condizionale (commentato per non crashare)
  // KLOG_ASSERT(test_value == 0, "This would trigger a panic!");

  kprintf("\n=== Real-world kernel logging examples ===\n");

  // Esempi realistici di logging kernel
  klog_info("Memory manager: initializing heap at %p", (void *)0x100000);
  klog_debug("Heap size: %zu bytes, alignment: %d", (size_t)1024 * 1024, 16);

  klog_info("Interrupt controller: setting up IDT");
  klog_debug("IDT base address: %p, limit: %u", (void *)0x50000, 256 * 8);

  while (1) {
    __asm__ volatile("hlt");
  }
}