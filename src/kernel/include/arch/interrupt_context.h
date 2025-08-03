#pragma once

/**
 * @file arch/interrupt_context.h
 * @brief Definizione del contesto CPU per interrupt
 *
 * Questo header fornisce un'astrazione architettura-indipendente per il
 * contesto CPU salvato durante un interrupt. Le implementazioni specifiche
 * dell'architettura devono includere questo file e definire il tipo
 * `arch_interrupt_context_t`.
 */

#if defined(__x86_64__) || defined(_M_X64)
#include <arch/x86_64/interrupt_context.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arch/arm64/interrupt_context.h>
#else
#error "Architettura non supportata per interrupt context"
#endif
