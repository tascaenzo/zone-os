#include <arch/interrupts.h>
#include <arch/cpu.h>
#include <arch/io.h>
#include <klib/klog.h>

#define IDT_ENTRIES 256
#define IDT_FLAG_INT_GATE 0x8E
#define KERNEL_CS 0x08

typedef struct __attribute__((packed)) {
    u16 offset_low;
    u16 selector;
    u8  ist;
    u8  type_attr;
    u16 offset_mid;
    u32 offset_high;
    u32 zero;
} idt_entry_t;

typedef struct __attribute__((packed)) {
    u16 limit;
    u64 base;
} idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_descriptor;

extern void *isr_stub_table[];
extern void isr_common_stub(void);

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

void idt_init(void) {
    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base = (u64)&idt;

    for (int i = 0; i < 48; ++i) {
        idt_set_gate(i, isr_stub_table[i], IDT_FLAG_INT_GATE);
    }

    __asm__ volatile("lidt %0" : : "m"(idt_descriptor));
    klog_info("IDT initialized with %d entries", 48);
}

/* -------------------------------- PIC ------------------------------------ */
#define PIC1            0x20
#define PIC2            0xA0
#define PIC1_COMMAND    PIC1
#define PIC1_DATA       (PIC1+1)
#define PIC2_COMMAND    PIC2
#define PIC2_DATA       (PIC2+1)
#define PIC_EOI         0x20

void pic_remap(u8 offset1, u8 offset2) {
    u8 a1 = inb(PIC1_DATA);
    u8 a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, offset1);
    outb(PIC2_DATA, offset2);

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_send_eoi(u8 irq) {
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

/* ----------------------------- ISR handler ------------------------------- */
void isr_common_handler(isr_frame_t *frame) {
    klog_warn("Interrupt vector %lu received", frame->int_no);

    if (frame->int_no >= 32 && frame->int_no <= 47) {
        pic_send_eoi(frame->int_no - 32);
    }
}
