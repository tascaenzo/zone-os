#pragma once
#include <arch/interrupt_context.h>

/**
 * @file kernel/panic.h
 * @brief Funzioni per la gestione di errori fatali nel kernel
 */

/**
 * @brief Arresta il sistema con un messaggio di errore (senza contesto CPU)
 *
 * @param msg Messaggio formattato (printf-style)
 */
__attribute__((noreturn)) void panic(const char *msg, ...);

/**
 * @brief Arresta il sistema con un messaggio di errore e stato CPU
 *
 * @param msg Messaggio formattato
 * @param ctx Puntatore al contesto registri salvato
 */
__attribute__((noreturn)) void panic_with_ctx(const char *msg, const arch_interrupt_context_t *ctx, ...);
