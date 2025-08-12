/**
 * @file arch/x86_64/gdt/gdt.h
 * @brief Strutture dati e API interne per la gestione della GDT e del TSS su x86_64
 *
 * Definisce i formati dei descrittori di segmento, del Task State Segment (TSS)
 * e della Global Descriptor Table (GDT) in modalità long mode.
 * Fornisce inoltre la dichiarazione della funzione di inizializzazione `gdt_init()`,
 * usata dal backend x86_64 per preparare la segmentazione di base.
 *
 * Strutture principali:
 * - GDT_Descriptor: descrittore standard da 8 byte (codice/dati/TSS low)
 * - TSS: struttura del Task State Segment secondo Intel SDM vol. 3, Fig. 7-11
 * - GDT: tabella completa, allineata a 4096 byte, con entry per kernel, utente e TSS
 * - GDT_Pointer: struttura usata dall’istruzione `lgdt`
 *
 * @note Questo header è interno al backend architetturale e non va incluso dal codice
 *       generico del kernel. Usare invece l’API pubblica `arch_segment_init()` per
 *       l’inizializzazione cross-arch.
 *
 * @date 2025
 * @author Enzo Tasca
 */

#pragma once
#include <lib/types.h>

// Descrittore di segmento GDT standard (8 byte)
struct GDT_Descriptor {
  u16 limit_15_0;
  u16 base_15_0;
  u8 base_23_16;
  u8 type;                  // tipo + attributi
  u8 limit_19_16_and_flags; // upper limit + flags (granularity, long mode, etc.)
  u8 base_31_24;
} __attribute__((packed));

// Task State Segment (struttura secondo Intel SDM, Fig 7-11)
struct TSS {
  u32 reserved0;
  u64 rsp0;
  u64 rsp1;
  u64 rsp2;
  u64 reserved1;
  u64 ist1;
  u64 ist2;
  u64 ist3;
  u64 ist4;
  u64 ist5;
  u64 ist6;
  u64 ist7;
  u64 reserved2;
  u16 reserved3;
  u16 iopb_offset;
} __attribute__((packed));

// GDT completa allineata a 4096 byte
struct __attribute__((aligned(4096))) GDT {
  struct GDT_Descriptor null;        // 0x00
  struct GDT_Descriptor kernel_code; // 0x08
  struct GDT_Descriptor kernel_data; // 0x10
  struct GDT_Descriptor null2;       // 0x18
  struct GDT_Descriptor user_data;   // 0x20
  struct GDT_Descriptor user_code;   // 0x28
  struct GDT_Descriptor ovmf_data;   // 0x30
  struct GDT_Descriptor ovmf_code;   // 0x38
  struct GDT_Descriptor tss_low;     // 0x40
  struct GDT_Descriptor tss_high;    // 0x48 (parte alta base TSS)
} __attribute__((packed));

// Puntatore GDTR (passato a lgdt)
struct GDT_Pointer {
  u16 limit;
  u64 base;
} __attribute__((packed));

// Init GDT
void gdt_init(void);
