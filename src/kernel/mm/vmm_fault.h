#pragma once
#include <interrupts/interrupts.h>
#include <lib/types.h>

/**
 * @brief Gestore per eccezioni di tipo Page Fault (#PF)
 *
 * @param fault_addr Indirizzo che ha generato il fault (CR2)
 * @param err_code Codice errore della CPU
 * @param ctx Contesto CPU completo salvato dallo stub
 * @return true se gestito, false se fatale
 */
bool vmm_handle_page_fault(u64 fault_addr, u64 err_code, arch_interrupt_context_t *ctx);
