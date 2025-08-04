#pragma once

/**
 * @file arch/interrupts.h
 * @brief Inclusione condizionale dell'interfaccia architetturale per interrupt
 *
 * Questo header include automaticamente il file `interrupts_arch.h`
 * corretto in base all'architettura di compilazione (x86_64, ARM64, etc).
 *
 * Deve essere incluso nei moduli portabili (es. kernel/interrupts.c)
 * per accedere all'interfaccia `arch_interrupts_*()` senza hardcode.
 */

#if defined(__x86_64__) || defined(_M_X64)
#include <arch/x86_64/interrupts_arch.h>
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arch/arm64/interrupts_arch.h>
#elif defined(__riscv) && (__riscv_xlen == 64)
#include <arch/riscv64/interrupts_arch.h>
#elif defined(__i386__) || defined(_M_IX86)
#include <arch/i386/interrupts_arch.h>
#else
#error "Architettura non supportata per <arch/interrupts.h>"
#endif
