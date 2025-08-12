/**
 * @file arch/x86_64/cpu_impl.c
 * @brief CPU architecture layer implementation (x86_64) for ZONE-OS
 *
 * Questo file implementa le API portabili dichiarate in <arch/cpu.h>
 * utilizzando istruzioni e registri specifici dell’architettura x86_64
 * (CPUID, MSR, CR*, INVLPG, ecc.). Il kernel deve includere solo l’API
 * neutra <arch/cpu.h>; i dettagli di questa implementazione sono privati
 * della piattaforma.
 *
 * Funzionalità coperte:
 *  - Power/idle & interrupt control (hlt/sti/cli/pause)
 *  - Rilevazione core/logical CPU e APIC ID
 *  - Memory ordering e sincronizzazione (mfence, pause)
 *  - Gestione TLB (invlpg) e indirizzo di fault (CR2)
 *  - Rilevazione feature (NX, SYSCALL/SYSRET)
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include <arch/cpu.h>
#include <lib/stdbool.h>
#include <lib/stdint.h>

/* ============================================================
 *  Helpers interni per CPUID e MSR
 * ============================================================ */
static inline void cpu_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
  __asm__ volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(subleaf));
}

static inline uint64_t cpu_rdmsr(uint32_t msr) {
  uint32_t lo, hi;
  __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
  return ((uint64_t)hi << 32) | lo;
}

static inline void cpu_wrmsr(uint32_t msr, uint64_t value) {
  uint32_t lo = (uint32_t)value;
  uint32_t hi = (uint32_t)(value >> 32);
  __asm__ volatile("wrmsr" ::"c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t cpu_read_cr2(void) {
  uint64_t val;
  __asm__ volatile("mov %%cr2, %0" : "=r"(val));
  return val;
}

static inline uint64_t cpu_read_cr3(void) {
  uint64_t val;
  __asm__ volatile("mov %%cr3, %0" : "=r"(val));
  return val;
}

static inline void cpu_write_cr3(uint64_t val) {
  __asm__ volatile("mov %0, %%cr3" ::"r"(val) : "memory");
}

/* ============================================================
 *  HALT / INTERRUPTS / PAUSE
 * ============================================================ */
void arch_cpu_halt(void) {
  __asm__ volatile("hlt");
}

void arch_cpu_enable_interrupts(void) {
  __asm__ volatile("sti");
}

void arch_cpu_disable_interrupts(void) {
  __asm__ volatile("cli");
}

void arch_cpu_pause(void) {
  __asm__ volatile("pause");
}

/* ============================================================
 *  CPU COUNT & ID
 * ============================================================ */
unsigned int arch_cpu_count(void) {
  uint32_t eax, ebx, ecx, edx;
  cpu_cpuid(0x0B, 0, &eax, &ebx, &ecx, &edx);
  if (ebx > 0) {
    return ebx; // numero di processori logici
  }

  /* TODO: Integrare parsing ACPI MADT per supporto reale SMP */
  return 1; // fallback
}

unsigned int arch_cpu_current_id(void) {
  uint32_t eax, ebx, ecx, edx;
  cpu_cpuid(0x01, 0, &eax, &ebx, &ecx, &edx);
  return (ebx >> 24) & 0xFF; // APIC ID
}

/* ============================================================
 *  CACHE / MEMORY ORDERING
 * ============================================================ */
void arch_cpu_flush_cache(void) {
  /* WBINVD invalida e scrive tutte le cache */
  __asm__ volatile("wbinvd");
}

void arch_cpu_memory_barrier(void) {
  __asm__ volatile("mfence" ::: "memory");
}

void arch_cpu_sync_barrier(void) {
  __asm__ volatile("mfence" ::: "memory");
  __asm__ volatile("pause");
}

/* ============================================================
 *  TLB MANAGEMENT
 * ============================================================ */
void arch_tlb_invalidate(void *virt_addr) {
  __asm__ volatile("invlpg (%0)" ::"r"(virt_addr) : "memory");
}

/* ============================================================
 *  FEATURE DETECTION
 * ============================================================ */
bool arch_cpu_has_nx(void) {
  uint32_t eax, ebx, ecx, edx;
  cpu_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
  return (edx >> 20) & 1; // Bit NX
}

bool arch_cpu_has_fast_syscall(void) {
  uint32_t eax, ebx, ecx, edx;
  cpu_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
  return (edx >> 11) & 1; // Bit SYSCALL/SYSRET
}

/* ============================================================
 *  FAULT ADDRESS
 * ============================================================ */
uintptr_t arch_cpu_fault_address(void) {
  return cpu_read_cr2();
}
