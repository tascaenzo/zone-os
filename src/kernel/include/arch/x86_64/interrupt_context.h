#pragma once

#include <lib/types.h>

/**
 * @file arch/x86_64/interrupt_context.h
 * @brief Definizione del contesto CPU per interrupt x86_64
 *
 * Questa struttura rappresenta lo stato completo della CPU al momento
 * della generazione di un interrupt o eccezione, come salvato manualmente
 * e automaticamente dallo stub ISR.
 *
 * Deve essere perfettamente allineata al layout creato in assembly:
 * - 15 registri generali pushati manualmente
 * - vettore + error code
 * - contesto automatico salvato dalla CPU (RIP, CS, RFLAGS, RSP, SS)
 */

typedef struct __attribute__((packed)) arch_interrupt_context {
  // --- Registri pushati manualmente dallo stub ---
  u64 rax;
  u64 rbx;
  u64 rcx;
  u64 rdx;
  u64 rbp;
  u64 rsi;
  u64 rdi;
  u64 r8;
  u64 r9;
  u64 r10;
  u64 r11;
  u64 r12;
  u64 r13;
  u64 r14;
  u64 r15;

  // --- Informazioni specifiche dell'interrupt ---
  u64 interrupt_vector; // pushato a mano o via macro
  u64 error_code;       // presente solo per alcune eccezioni

  // --- Stato salvato automaticamente dalla CPU ---
  u64 rip;
  u64 cs;
  u64 rflags;
  u64 rsp;
  u64 ss;
} arch_interrupt_context_t;
