// #include <arch/cpu.h>
// #include <arch/interrupt_context.h>
// #include <klib/klog.h>
// #include <lib/types.h>
// #include <mm/pmm.h>
// #include <mm/vmm.h>
//
// #define VMM_FAULT_STACK_MAX_GROWTH (32 * PAGE_SIZE)
//
// bool vmm_handle_page_fault(u64 fault_addr, u64 err_code, arch_interrupt_context_t *ctx) {
//   // Protezione base: null pointer
//   if (fault_addr < 0x1000) {
//     klog_error("[#PF] Null pointer access (addr=0x%lx)", fault_addr);
//     return false;
//   }
//
//   // Allinea l’indirizzo a pagina
//   u64 page_addr = fault_addr & ~(PAGE_SIZE - 1);
//
//   // Determina tipo accesso (solo per log/debug)
//   bool is_present = err_code & 0x1;
//   bool is_write = err_code & 0x2;
//   bool is_user = err_code & 0x4;
//   bool is_exec = err_code & 0x10;
//
//   klog_debug("[#PF] Fault addr = %p (present=%d write=%d user=%d exec=%d)", (void *)fault_addr, is_present, is_write, is_user, is_exec);
//
//   // Caso 1: pagina non presente → allochiamola (lazy alloc)
//   if (!is_present) {
//     void *new_page = pmm_alloc_page();
//     if (!new_page) {
//       klog_error("[#PF] Impossibile allocare nuova pagina");
//       return false;
//     }
//
//     u64 flags = VMM_FLAG_READ | VMM_FLAG_WRITE;
//
//     if (is_user)
//       flags |= VMM_FLAG_USER;
//
//     bool ok = vmm_map(NULL, page_addr, (u64)new_page, 1, flags);
//     if (!ok) {
//       klog_error("[#PF] Mapping fallito per nuova pagina");
//       return false;
//     }
//
//     klog_info("[#PF] Risolta con mappatura dinamica (%p)", new_page);
//     return true;
//   }
//
//   // Caso 2: protezione (scrittura su read-only / NX / accesso user→kernel)
//   klog_error("[#PF] Violazione di protezione — RIP=%p", (void *)ctx->rip);
//   return false;
// }
//