#include <arch/cpu.h>
#include <arch/interrupts.h>
#include <arch/io.h>
#include <klib/klog.h>

/*
 * ============================================================================
 * IDT (Interrupt Descriptor Table) SETUP
 * ============================================================================
 */

#define IDT_ENTRIES 256
#define IRQ_COUNT 16
#define IDT_FLAG_INT_GATE 0x8E
#define KERNEL_CS 0x08

typedef struct __attribute__((packed)) {
  u16 offset_low;  // [0:15]   Bits bassi indirizzo ISR
  u16 selector;    //         Segment selector (es. KERNEL_CS)
  u8 ist;          //         Interrupt Stack Table (non usato)
  u8 type_attr;    //         Tipo (interrupt gate) e flag
  u16 offset_mid;  // [16:31]  Bits medi indirizzo ISR
  u32 offset_high; // [32:63]  Bits alti indirizzo ISR
  u32 zero;        //         Riservato
} idt_entry_t;

typedef struct __attribute__((packed)) {
  u16 limit;
  u64 base;
} idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_descriptor;

extern void *isr_stub_table[];     // Tabella definita in isr_stub.s
extern void isr_common_stub(void); // Stub assembly condiviso per gli ISR

// Imposta una entry dell'IDT
static void idt_set_gate(int n, void *isr, u8 flags) {
  u64 addr = (u64)isr;
  idt[n].offset_low = addr & 0xFFFF;
  idt[n].selector = KERNEL_CS;
  idt[n].ist = 0;
  idt[n].type_attr = flags;
  idt[n].offset_mid = (addr >> 16) & 0xFFFF;
  idt[n].offset_high = (addr >> 32) & 0xFFFFFFFF;
  idt[n].zero = 0;
}

// Inizializza l’IDT con gli stub di interrupt
void idt_init(void) {
  idt_descriptor.limit = sizeof(idt) - 1;
  idt_descriptor.base = (u64)&idt;

  for (int i = 0; i < 48; ++i) {
    idt_set_gate(i, isr_stub_table[i], IDT_FLAG_INT_GATE);
  }

  __asm__ volatile("lidt %0" : : "m"(idt_descriptor));
  klog_info("IDT initialized with %d entries", 48);
}

/*
 * ============================================================================
 * PIC (Programmable Interrupt Controller) SETUP
 * ============================================================================
 */

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2 + 1)
#define PIC_EOI 0x20

// Remappa i vettori IRQ da [0-15] a [32-47]
void pic_remap(u8 offset1, u8 offset2) {
  u8 a1 = inb(PIC1_DATA); // Salva maschere attuali
  u8 a2 = inb(PIC2_DATA);

  outb(PIC1_COMMAND, 0x11); // Init
  outb(PIC2_COMMAND, 0x11);

  outb(PIC1_DATA, offset1); // Offset PIC1 = 0x20 (32)
  outb(PIC2_DATA, offset2); // Offset PIC2 = 0x28 (40)

  outb(PIC1_DATA, 4); // Informa che PIC2 è su IRQ2
  outb(PIC2_DATA, 2);

  outb(PIC1_DATA, 0x01); // Modalità 8086
  outb(PIC2_DATA, 0x01);

  outb(PIC1_DATA, a1); // Ripristina maschere originali
  outb(PIC2_DATA, a2);
}

// Invia EOI (End Of Interrupt) al PIC dopo la gestione di un IRQ
void pic_send_eoi(u8 irq) {
  if (irq >= 8)
    outb(PIC2_COMMAND, PIC_EOI);
  outb(PIC1_COMMAND, PIC_EOI);
}

/*
 * ============================================================================
 * IRQ HANDLER SYSTEM (Registrazione dinamica handler IRQ)
 * ============================================================================
 */

typedef void (*irq_handler_t)(isr_frame_t *);
static irq_handler_t irq_handlers[IRQ_COUNT] = {0};

// Permette al kernel di registrare un handler per un IRQ specifico
void irq_register_handler(int irq, irq_handler_t handler) {
  if (irq >= 0 && irq < IRQ_COUNT) {
    irq_handlers[irq] = handler;
    klog_info("IRQ %d handler registered", irq);
  }
}

/*
 * ============================================================================
 * ISR COMMON HANDLER
 * ============================================================================
 */

// Handler centrale chiamato da isr_common_stub (in ASM)
void isr_common_handler(isr_frame_t *frame) {
  u64 int_no = frame->int_no;

  if (int_no >= 32 && int_no <= 47) {
    int irq = int_no - 32;

    if (irq_handlers[irq]) {
      irq_handlers[irq](frame);
    } else {
      klog_warn("Unhandled IRQ %d", irq);
    }

    pic_send_eoi(irq);
  } else {
    klog_warn("CPU Exception %lu occurred (not handled)", int_no);
  }
}
