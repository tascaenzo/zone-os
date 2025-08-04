#pragma once

/**
 * @file arch/x86_64/interrupts_arch.h
 * @brief Interfaccia architettura-specifica per la gestione degli interrupt (x86_64)
 *
 * Questo header definisce le primitive arch-specifiche necessarie per
 * inizializzare e gestire la tabella degli interrupt (IDT), il dispatch
 * verso gli handler, e il controllo globale degli interrupt per x86_64.
 *
 * Il kernel core non dovrebbe usare direttamente queste funzioni:
 * deve invece interfacciarsi tramite <kernel/interrupts.h>.
 */

#include <arch/interrupt_context.h> // Definizione di arch_interrupt_context_t
#include <lib/types.h>

/**
 * @brief Tipo della funzione handler per interrupt a livello architetturale
 *
 * Ogni handler riceve il contesto completo della CPU al momento
 * dell'interrupt, insieme al numero del vettore che lo ha generato.
 */
typedef void (*arch_interrupt_handler_t)(arch_interrupt_context_t *ctx, u8 vector);

/**
 * @brief Inizializza il sistema di interrupt su x86_64
 *
 * Alloca e popola la IDT, collega gli stub assembly (`isr_stub_table`),
 * e carica il registro IDT tramite l'istruzione `lidt`.
 */
void arch_interrupts_init(void);

/**
 * @brief Registra un handler C per un vettore specifico (0–255)
 *
 * @param vector  Numero del vettore (es. 0 = Divide by zero, 32 = IRQ0)
 * @param handler Funzione da eseguire in risposta all'interrupt
 * @return 0 su successo, -1 se già presente o non valido
 */
int arch_interrupts_register_handler(u8 vector, arch_interrupt_handler_t handler);

/**
 * @brief Rimuove un handler registrato in precedenza
 *
 * @param vector Vettore da liberare
 * @return 0 su successo, -1 se fallisce
 */
int arch_interrupts_unregister_handler(u8 vector);

/**
 * @brief Restituisce una stringa simbolica che descrive un vettore di eccezione
 *
 * Utile per messaggi di errore o debugging (es. "Page Fault", "GPF", etc).
 *
 * @param vector Numero del vettore (0–31 tipicamente)
 * @return Puntatore a stringa statica, o "Unknown" se non noto
 */
const char *arch_interrupt_exception_name(u8 vector);

/**
 * @brief Punto di ingresso chiamato dallo stub ASM per inoltrare l'interrupt al C
 *
 * Questa funzione è invocata da `isr_common_stub` con i due argomenti:
 * - il numero del vettore
 * - il puntatore al contesto CPU (`arch_interrupt_context_t`)
 *
 * Si occupa di chiamare l'handler registrato se presente.
 *
 * @param vector Numero vettore (0–255)
 * @param ctx    Stato CPU salvato
 */
arch_interrupt_context_t *arch_interrupts_dispatch(u8 vector, arch_interrupt_context_t *ctx);
