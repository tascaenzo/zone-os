#include <arch/memory.h>
#include <bootloader/limine.h>
#include <drivers/video/console.h>
#include <drivers/video/framebuffer.h>
#include <klib/klog.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <mm/heap/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>

/*
 * ============================================================================
 * BOOTLOADER REQUESTS
 * ============================================================================
 */

volatile struct limine_framebuffer_request framebuffer_request = {.id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

/*
 * ============================================================================
 * HEAP TEST FUNCTIONS
 * ============================================================================
 */

/**
 * @brief Test di base per il heap ibrido
 */
void test_heap_basic(void) {
  klog_info("=== HEAP BASIC TESTS ===");

  // Test 1: Allocazioni piccole (slab)
  void *small1 = kalloc(64);
  void *small2 = kalloc(128);
  void *small3 = kalloc(512);

  if (small1 && small2 && small3) {
    klog_info("Small allocations successful (64B, 128B, 512B)");

    // Test scrittura
    memset(small1, 0xAA, 64);
    memset(small2, 0xBB, 128);
    memset(small3, 0xCC, 512);

    // Verifica che i dati siano corretti
    if (*(u8 *)small1 == 0xAA && *(u8 *)small2 == 0xBB && *(u8 *)small3 == 0xCC) {
      klog_info("Memory write/read test passed");
    } else {
      klog_error("Memory write/read test failed");
    }
  } else {
    klog_error("Small allocations failed");
  }

  // Test 2: Allocazioni grandi (buddy - se implementato)
  void *large1 = kalloc(4096); // 4KB
  void *large2 = kalloc(8192); // 8KB

  if (large1 && large2) {
    klog_info("Large allocations successful (4KB, 8KB)");
  } else {
    klog_warn("Large allocations failed (buddy allocator not implemented)");
  }

  // Test 3: kcalloc (zero-initialized)
  void *zero_mem = kcalloc(100, sizeof(u32));
  if (zero_mem) {
    bool all_zero = true;
    u32 *arr = (u32 *)zero_mem;
    for (int i = 0; i < 100; i++) {
      if (arr[i] != 0) {
        all_zero = false;
        break;
      }
    }

    if (all_zero) {
      klog_info("✓ kcalloc zero-initialization test passed");
    } else {
      klog_error("✗ kcalloc zero-initialization test failed");
    }
  }

  // Test 4: kalloc_flags con HEAP_FLAG_ZERO
  void *zero_flag = kalloc_flags(256, HEAP_FLAG_ZERO, 0);
  if (zero_flag) {
    bool all_zero = true;
    u8 *bytes = (u8 *)zero_flag;
    for (int i = 0; i < 256; i++) {
      if (bytes[i] != 0) {
        all_zero = false;
        break;
      }
    }

    if (all_zero) {
      klog_info("✓ kalloc_flags ZERO test passed");
    } else {
      klog_error("✗ kalloc_flags ZERO test failed");
    }
  }

  // Test 5: Deallocazioni
  kfree(small1);
  kfree(small2);
  kfree(small3);
  kfree(large1);
  kfree(large2);
  kfree(zero_mem);
  kfree(zero_flag);

  klog_info("✓ All deallocations completed");
}

/**
 * @brief Test di stress per il heap
 */
void test_heap_stress(void) {
  klog_info("=== HEAP STRESS TESTS ===");

  const int NUM_ALLOCS = 50;
  void *ptrs[NUM_ALLOCS];
  size_t sizes[] = {16, 32, 64, 128, 256, 512, 1024, 2048};
  const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

  // Test 1: Allocazioni multiple
  int successful_allocs = 0;
  for (int i = 0; i < NUM_ALLOCS; i++) {
    size_t size = sizes[i % num_sizes];
    ptrs[i] = kalloc(size);
    if (ptrs[i]) {
      successful_allocs++;
      // Scrivi pattern per test integrità
      memset(ptrs[i], (u8)(i & 0xFF), size);
    }
  }

  klog_info("Allocated %d/%d blocks successfully", successful_allocs, NUM_ALLOCS);

  // Test 2: Verifica integrità dati
  int integrity_ok = 0;
  for (int i = 0; i < NUM_ALLOCS; i++) {
    if (ptrs[i]) {
      u8 expected = (u8)(i & 0xFF);
      if (*(u8 *)ptrs[i] == expected) {
        integrity_ok++;
      }
    }
  }

  klog_info("Data integrity: %d/%d blocks OK", integrity_ok, successful_allocs);

  // Test 3: Deallocazioni random
  for (int i = 0; i < NUM_ALLOCS; i += 2) {
    if (ptrs[i]) {
      kfree(ptrs[i]);
      ptrs[i] = NULL;
    }
  }

  // Test 4: Riallocazioni negli slot liberati
  int reallocs = 0;
  for (int i = 0; i < NUM_ALLOCS; i += 2) {
    size_t size = sizes[i % num_sizes];
    ptrs[i] = kalloc(size);
    if (ptrs[i]) {
      reallocs++;
    }
  }

  klog_info("Reallocated %d blocks in freed slots", reallocs);

  // Test 5: Cleanup finale
  for (int i = 0; i < NUM_ALLOCS; i++) {
    if (ptrs[i]) {
      kfree(ptrs[i]);
    }
  }

  klog_info("✓ Stress test cleanup completed");
}

/**
 * @brief Test krealloc
 */
void test_heap_realloc(void) {
  klog_info("=== HEAP REALLOC TESTS ===");

  // Test 1: Espansione
  void *ptr = kalloc(100);
  if (ptr) {
    memset(ptr, 0xAA, 100);

    ptr = krealloc(ptr, 200);
    if (ptr && *(u8 *)ptr == 0xAA) {
      klog_info("✓ krealloc expansion test passed");
    } else {
      klog_error("✗ krealloc expansion test failed");
    }
  }

  // Test 2: Contrazione
  if (ptr) {
    ptr = krealloc(ptr, 50);
    if (ptr && *(u8 *)ptr == 0xAA) {
      klog_info("✓ krealloc shrink test passed");
    } else {
      klog_error("✗ krealloc shrink test failed");
    }
  }

  // Test 3: Cambio categoria (slab -> buddy o viceversa)
  if (ptr) {
    ptr = krealloc(ptr, 4096); // Forza buddy allocation
    if (ptr) {
      klog_info("krealloc category change test completed");
    } else {
      klog_warn("krealloc category change failed (buddy not implemented)");
    }
  }

  // Cleanup
  kfree(ptr);

  // Test 4: krealloc con NULL (equivale a kalloc)
  ptr = krealloc(NULL, 256);
  if (ptr) {
    klog_info("krealloc(NULL) test passed");
    kfree(ptr);
  }

  // Test 5: krealloc con size 0 (equivale a kfree)
  ptr = kalloc(128);
  ptr = krealloc(ptr, 0);
  if (ptr == NULL) {
    klog_info("✓ krealloc(ptr, 0) test passed");
  }
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
   * FASE 4: INIZIALIZZAZIONE HEAP IBRIDO
   */
  klog_info("Initializing Hybrid Heap (Slab + Buddy)...");
  heap_init();
  klog_info("Heap initialized successfully");

  // Verifica integrità heap dopo init
  if (heap_check_integrity()) {
    klog_info("Heap integrity check passed");
  } else {
    klog_error("Heap integrity check failed");
  }

  /*
   * FASE 5: TEST HEAP COMPLETI
   */
  klog_info("Running comprehensive heap tests...");

  test_heap_basic();
  test_heap_stress();
  test_heap_realloc();

  // Verifica integrità finale
  if (heap_check_integrity()) {
    klog_info("Final heap integrity check passed");
  } else {
    klog_error("Final heap integrity check failed");
  }

  // Statistiche finali heap
  heap_dump_stats();

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