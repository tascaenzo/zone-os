#pragma once
#include <lib/types.h>

/**
 * @brief Struttura che rappresenta lo stato del processore al momento dell'interrupt
 *
 * Questa struttura viene popolata automaticamente dagli stub ISR quando
 * un'interruzione (hardware o software) viene gestita. Serve per accedere
 * ai registri salvati all'interno dell'handler in C.
 */
typedef struct __attribute__((packed)) {
  // Registri generali salvati manualmente nello stub
  u64 r15, r14, r13, r12, r11, r10, r9, r8;
  u64 rsi, rdi, rbp, rdx, rcx, rbx, rax;

  // Informazioni sull'interrupt (pushed dallo stub)
  u64 int_no;   ///< Numero del vettore interrupt
  u64 err_code; ///< Codice errore (solo per ISR con errore)

  // Stato del processore salvato automaticamente dalla CPU
  u64 rip;    ///< Instruction Pointer al momento dell'interrupt
  u64 cs;     ///< Code Segment
  u64 rflags; ///< Flags della CPU
  u64 rsp;    ///< Stack Pointer
  u64 ss;     ///< Stack Segment
} isr_frame_t;

/**
 * @brief Inizializza la IDT (Interrupt Descriptor Table)
 */
void idt_init(void);

/**
 * @brief Rimappa il PIC per evitare conflitti con gli interrupt della CPU
 *
 * @param offset1 Offset base per IRQ del PIC master (di solito 0x20)
 * @param offset2 Offset base per IRQ del PIC slave (di solito 0x28)
 */
void pic_remap(u8 offset1, u8 offset2);

/**
 * @brief Invia End Of Interrupt al PIC
 *
 * @param irq Numero IRQ gestito (necessario per riabilitare il prossimo IRQ)
 */
void pic_send_eoi(u8 irq);

/**
 * @brief Handler comune in C per tutti gli interrupt
 *
 * Questo viene invocato da `isr_common_stub` e riceve lo snapshot del contesto
 */
void isr_common_handler(isr_frame_t *frame);
