#pragma once

#include <lib/stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Puntatore alla tabella degli stub ISR (esportata da isr_stub.s)
 *        Contiene 256 voci (max IDT).
 */
extern void *isr_stub_table[];

#ifdef __cplusplus
}
#endif
