#include <arch/cpu.h>
#include <arch/x86_64/interrupt_context.h>
#include <arch/x86_64/interrupts_arch.h>
#include <arch/x86_64/isr_stub.h>
#include <klib/klog.h>
#include <lib/string.h>

#define IDT_ENTRIES 256

// -----------------------------
// Strutture IDT
// -----------------------------

struct __attribute__((packed)) idt_entry {
  u16 offset_low;
  u16 selector;
  u8 ist;
  u8 type_attr;
  u16 offset_mid;
  u32 offset_high;
  u32 zero;
};

struct __attribute__((packed)) idt_ptr {
  u16 limit;
  u64 base;
};

// -----------------------------
// Tabelle statiche
// -----------------------------

static struct idt_entry idt[IDT_ENTRIES];
static arch_interrupt_handler_t handlers[IDT_ENTRIES];

// Definita in isr_stub.s
extern void *isr_stub_table[];

// -----------------------------
// Funzione di utilità: set IDT
// -----------------------------

static void idt_set_gate(int vector, void *isr) {
  u64 addr = (u64)isr;

  struct idt_entry *entry = &idt[vector];

  entry->offset_low = addr & 0xFFFF;
  entry->selector = 0x08;  // Segmento codice del kernel (GDT)
  entry->ist = 0;          // Nessun cambio di stack (IST = 0)
  entry->type_attr = 0x8E; // Interrupt gate, present, ring 0
  entry->offset_mid = (addr >> 16) & 0xFFFF;
  entry->offset_high = (addr >> 32) & 0xFFFFFFFF;
  entry->zero = 0;
}

// -----------------------------
// Inizializzazione IDT
// -----------------------------

void arch_interrupts_init(void) {
  memset(idt, 0, sizeof(idt));
  memset(handlers, 0, sizeof(handlers));

  for (int vec = 0; vec < IDT_ENTRIES; vec++) {
    idt_set_gate(vec, isr_stub_table[vec]);
  }

  for (int i = 0; i < 5; ++i) {
    klog_warn("isr_stub[%d] = %p", i, isr_stub_table[i]);
  }

  struct idt_ptr idtp = {
      .limit = sizeof(idt) - 1,
      .base = (u64)&idt,
  };

  asm volatile("lidt %0" : : "m"(idtp));
  klog_info("IDT loaded (base=%p, entries=%u)", &idt, IDT_ENTRIES);
}

// -----------------------------
// Registrazione handler
// -----------------------------

int arch_interrupts_register_handler(u8 vector, arch_interrupt_handler_t handler) {
  if (handlers[vector] != NULL)
    return -1;

  handlers[vector] = handler;
  klog_warn("Handler registrato per vettore %u (handler=%p)", vector, handler); // 👈 AGGIUNGI QUI
  return 0;
}

int arch_interrupts_unregister_handler(u8 vector) {
  handlers[vector] = (arch_interrupt_handler_t)NULL;
  return 0;
}

// -----------------------------
// Dispatcher chiamato da stub ASM
// -----------------------------

void arch_interrupts_dispatch(u8 vector, arch_interrupt_context_t *ctx) {
  klog_debug("DISPATCH vector=%u RIP=%p", vector, ctx->rip);

  if (handlers[vector]) {
    handlers[vector](ctx, vector);
  } else {
    klog_panic("Unhandled interrupt %u (RIP=%p)", vector, ctx->rip);
    while (1)
      asm volatile("hlt");
  }
}

// -----------------------------
// Nomi eccezioni (debug/log)
// -----------------------------

static const char *exception_names[] = {"Divide by Zero",
                                        "Debug",
                                        "NMI",
                                        "Breakpoint",
                                        "Overflow",
                                        "Bound Range Exceeded",
                                        "Invalid Opcode",
                                        "Device Not Available",
                                        "Double Fault",
                                        "Coprocessor Segment Overrun",
                                        "Invalid TSS",
                                        "Segment Not Present",
                                        "Stack Fault",
                                        "General Protection Fault",
                                        "Page Fault",
                                        "Reserved",
                                        "FPU Floating Point",
                                        "Alignment Check",
                                        "Machine Check",
                                        "SIMD FP Exception",
                                        "Virtualization",
                                        "Control Protection",
                                        "Reserved",
                                        "Reserved",
                                        "Hypervisor Injection",
                                        "VMM Comm",
                                        "Security Exception",
                                        "Triple Fault",
                                        "FPU Error",
                                        "Reserved"};

const char *arch_interrupt_exception_name(u8 vector) {
  if (vector < sizeof(exception_names) / sizeof(char *))
    return exception_names[vector];
  return "Unknown";
}
