#include <arch/cpu.h>
#include <interrupts/exceptions.h>
#include <interrupts/interrupts.h>
#include <klib/klog.h>
#include <klib/panic.h>
#include <lib/string.h>
#include <mm/vmm_fault.h>

/**
 * @file kernel/exceptions.c
 * @brief Gestione delle CPU exceptions (arch-indipendente)
 *
 * Questo modulo registra un handler di default per tutte le eccezioni
 * della CPU (vettori 0–31) e ne gestisce il comportamento caso per caso.
 */

/**
 * @brief Gestore generico per tutte le eccezioni CPU
 */
static void exception_handler(arch_interrupt_context_t *ctx, u8 vec) {
  const char *name = interrupts_exception_name(vec);

  switch (vec) {
  // Non fatali → warning e continua
  case 1: // Debug
  case 3: // Breakpoint
  case 4: // Overflow
    klog_warn("[INT %u] %s at RIP=%p — continuing", vec, name, ctx->rip);
    return;

  case 7: // Device Not Available (FPU lazy init)
    klog_warn("[#NM] FPU context switch richiesto — ignorato per ora");
    return;

  case 16:
  case 19: // FP/SIMD
    klog_warn("[#MF/#XF] FP/SIMD exception — ignorata");
    return;

  // Page fault: prova a gestirla con il VMM
  case 14: {
    u64 cr2 = cpu_read_cr2(); // Indirizzo causante l'eccezione

    if (vmm_handle_page_fault(cr2, ctx->error_code, ctx)) {
      klog_info("[#PF] Gestito: CR2=0x%lx RIP=%p", cr2, ctx->rip);
      return;
    }

    panic_with_ctx("[#PF] Page Fault non gestito - CR2=0x%lx RIP=%p ERR=0x%lx", ctx, cr2, ctx->rip, ctx->error_code);
  }

  // Tutto il resto → fatal
  default:
    panic_with_ctx("[#%u] %s - Fatal exception at RIP=%p ERR=0x%lx", ctx, vec, name, ctx->rip, ctx->error_code);
  }
}

/**
 * @brief Inizializza e registra il gestore per le eccezioni CPU
 */
void exceptions_init(void) {
  for (u8 vec = 0; vec < 32; vec++) {
    interrupts_register_handler(vec, exception_handler);
  }
  klog_info("Gestori delle eccezioni CPU registrati");
}
