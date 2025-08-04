#include <arch/cpu.h>
#include <arch/interrupt_context.h>
#include <arch/interrupts_arch.h>
#include <interrupts/interrupts.h>

/**
 * @file kernel/interrupts.c
 * @brief Dispatcher kernel-agnostico per la gestione degli interrupt
 *
 * Questo modulo fornisce un'implementazione generica delle API per
 * la gestione degli interrupt, inoltrando le chiamate alle funzioni
 * architettura-specifiche definite in <arch/interrupts.h>.
 */

void interrupts_init(void) {
  arch_interrupts_init();
}

void interrupts_enable(void) {
  cpu_enable_interrupts();
}

void interrupts_disable(void) {
  cpu_disable_interrupts();
}

int interrupts_register_handler(u8 vector, interrupt_handler_t handler) {
  return arch_interrupts_register_handler(vector, handler);
}

int interrupts_unregister_handler(u8 vector) {
  return arch_interrupts_unregister_handler(vector);
}

const char *interrupts_exception_name(u8 vector) {
  return arch_interrupt_exception_name(vector);
}
