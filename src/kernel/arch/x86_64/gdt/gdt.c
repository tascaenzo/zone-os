#include "gdt.h"
#include <klib/klog/klog.h>
#include <lib/string/string.h>

extern void _load_gdt_and_tss_asm(struct GDT_Pointer *ptr);

static struct TSS tss;
static struct GDT gdt;
static struct GDT_Pointer gdt_pointer;

// Imposta i campi di un descrittore GDT standard
static void set_descriptor(struct GDT_Descriptor *desc, u32 base, u32 limit, u8 type, u8 flags) {
  desc->limit_15_0 = limit & 0xFFFF;
  desc->base_15_0 = base & 0xFFFF;
  desc->base_23_16 = (base >> 16) & 0xFF;
  desc->type = type;
  desc->limit_19_16_and_flags = ((limit >> 16) & 0x0F) | (flags & 0xF0);
  desc->base_31_24 = (base >> 24) & 0xFF;
}

// Crea tutti i descrittori
static void create_descriptors(void) {
  set_descriptor(&gdt.null, 0, 0, 0x00, 0x00);                  // null
  set_descriptor(&gdt.kernel_code, 0, 0, 0x9A, 0xA0);           // code: exec/read, ring0, long mode
  set_descriptor(&gdt.kernel_data, 0, 0, 0x92, 0xA0);           // data: read/write, ring0
  set_descriptor(&gdt.null2, 0, 0, 0x00, 0x00);                 // null
  set_descriptor(&gdt.user_data, 0, 0, 0x92, 0xA0);             // data: ring3
  set_descriptor(&gdt.user_code, 0, 0, 0x9A, 0xA0);             // code: ring3
  set_descriptor(&gdt.ovmf_data, 0, 0, 0x92, 0xA0);             // compat
  set_descriptor(&gdt.ovmf_code, 0, 0, 0x9A, 0xA0);             // compat
  set_descriptor(&gdt.tss_low, 0, sizeof(tss) - 1, 0x89, 0xA0); // TSS: present, system type 64-bit
  set_descriptor(&gdt.tss_high, 0, 0, 0x00, 0x00);              // parte alta (non usata come descrittore)
}

// Inizializzazione della GDT e caricamento TSS
void gdt_init(void) {
  create_descriptors();

  // Azzeramento TSS
  memset(&tss, 0, sizeof(struct TSS));

  // Imposta base del TSS nei due descrittori (basso e alto)
  uint64_t tss_base = (uint64_t)&tss;
  gdt.tss_low.base_15_0 = tss_base & 0xFFFF;
  gdt.tss_low.base_23_16 = (tss_base >> 16) & 0xFF;
  gdt.tss_low.base_31_24 = (tss_base >> 24) & 0xFF;
  gdt.tss_high.base_15_0 = (tss_base >> 32) & 0xFFFF;
  gdt.tss_high.limit_15_0 = (tss_base >> 48) & 0xFFFF;

  // GDTR pointer
  gdt_pointer.limit = sizeof(gdt) - 1;
  gdt_pointer.base = (uint64_t)&gdt;

  // Carica GDT + TSS
  _load_gdt_and_tss_asm(&gdt_pointer);

  // Log
  klog_info("GDT initialized\n");
}
