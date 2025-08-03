#pragma once

/**
 * @file kernel/interrupts.h
 * @brief Interfaccia generica per la gestione degli interrupt lato kernel
 *
 * Questo header fornisce al kernel un'astrazione architettura-indipendente
 * per registrare e gestire gli interrupt (eccezioni e IRQ hardware).
 * L'implementazione concreta è delegata al backend arch-specifico.
 */

#include <arch/interrupt_context.h> // Definizione arch-specifica del contesto
#include <lib/types.h>              // Tipi base come u8, u64

/**
 * @brief Tipo astratto del contesto CPU salvato durante un interrupt
 *
 * Ogni architettura fornisce la propria definizione concreta tramite
 * <arch/interrupt_context.h>.
 */
typedef struct arch_interrupt_context arch_interrupt_context_t;

/**
 * @brief Tipo della funzione handler invocata su interrupt
 *
 * Il kernel o i driver possono registrare un handler per un determinato
 * vettore. L'handler riceve un puntatore al contesto CPU e il numero
 * del vettore che ha generato l'interrupt.
 */
typedef void (*interrupt_handler_t)(arch_interrupt_context_t *ctx, u8 vector);

/**
 * @brief Inizializza il sistema di gestione interrupt (IDT/GIC, vettori, etc.)
 */
void interrupts_init(void);

/**
 * @brief Abilita globalmente gli interrupt (es. esecuzione di `sti`)
 */
void interrupts_enable(void);

/**
 * @brief Disabilita globalmente gli interrupt (es. esecuzione di `cli`)
 */
void interrupts_disable(void);

/**
 * @brief Registra un handler per un determinato vettore (0–255)
 *
 * @param vector  Numero del vettore da gestire
 * @param handler Puntatore alla funzione handler
 * @return 0 su successo, -1 su errore (es. handler già registrato)
 */
int interrupts_register_handler(u8 vector, interrupt_handler_t handler);

/**
 * @brief Rimuove un handler precedentemente registrato
 *
 * @param vector Vettore da liberare
 * @return 0 su successo, -1 su errore
 */
int interrupts_unregister_handler(u8 vector);

/**
 * @brief Ritorna il nome simbolico associato a un vettore di eccezione
 *
 * Utile per debugging o panic handler (es. "Page Fault", "GPF", etc.).
 *
 * @param vector Numero del vettore
 * @return Puntatore a stringa statica
 */
const char *interrupts_exception_name(u8 vector);
