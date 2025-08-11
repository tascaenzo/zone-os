/**
 * @file arch/x86_64/cpu/cpu_lowlevel.h
 * @brief Primitivi low-level CPU per backend x86_64 (CPUID/MSR/feature helpers) — header PRIVATO, non esporre in include/arch/*
 *
 * Nota: questo header fornisce intrinsics e helper specifici x86_64 destinati
 * all’implementazione del layer arch (es. arch/x86_64/memory.c, cpu.c). Il core
 * del kernel deve usare solo l’API portabile dichiarata in <arch/*.h>.
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once
#include <lib/stdbool.h>
#include <lib/stdint.h>

#if !defined(__x86_64__)
#error "cpu_lowlevel.h è solo per backend x86_64"
#endif

/* --------------------------------------------------------------------------
 * CPUID
 * --------------------------------------------------------------------------
 * Esegue l’istruzione CPUID con (leaf, subleaf).
 * Scrive EAX/EBX/ECX/EDX se i puntatori non sono NULL.
 * Sicuro con -fPIC su x86_64 (si può usare "=b").
 */
static inline void cpu_cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
  uint32_t a, b, c, d;
  __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf) : "memory");
  if (eax)
    *eax = a;
  if (ebx)
    *ebx = b;
  if (ecx)
    *ecx = c;
  if (edx)
    *edx = d;
}

/* --------------------------------------------------------------------------
 * MSR (Model Specific Registers)
 * --------------------------------------------------------------------------
 * Lettura/scrittura MSR. Richiedono privilegi ring0.
 */
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

/* --------------------------------------------------------------------------
 * Feature helpers (CPUID-derived)
 * --------------------------------------------------------------------------
 * Helper minimali usati dal backend arch per sanity-check e policy locali.
 * Non esporli al core: se serve dal core, introdurre una API portabile in <arch/cpu.h>.
 */
static inline bool cpu_has_msr(void) {
  uint32_t a, b, c, d;
  cpu_cpuid(1, 0, &a, &b, &c, &d);
  return (d & (1u << 5)) != 0; /* MSR present */
}

static inline bool cpu_has_nx(void) {
  uint32_t a, b, c, d;
  cpu_cpuid(0x80000001u, 0, &a, &b, &c, &d);
  return (d & (1u << 20)) != 0; /* NX bit */
}

static inline unsigned cpu_phys_addr_bits(void) {
  uint32_t a, b, c, d;
  cpu_cpuid(0x80000008u, 0, &a, &b, &c, &d);
  return a & 0xFFu; /* EAX[7:0] */
}

/* CPUID su x86_64 è sempre disponibile. Manteniamo l’helper per simmetria. */
static inline bool cpu_has_cpuid(void) {
  return true;
}

/* --------------------------------------------------------------------------
 * CR2 (fault address) — utility spesso usata da backend memoria/exception
 * -------------------------------------------------------------------------- */
static inline uint64_t cpu_read_cr2(void) {
  uint64_t v;
  __asm__ volatile("mov %%cr2,%0" : "=r"(v));
  return v;
}

/* --------------------------------------------------------------------------
 * Sentry di coerenza (opzionale): force-inline per evitare call site pubblici
 * -------------------------------------------------------------------------- */
#if defined(__GNUC__)
#define CPU_LL_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define CPU_LL_ALWAYS_INLINE inline
#endif
