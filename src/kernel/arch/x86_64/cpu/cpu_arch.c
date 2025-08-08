#include "cpu.h"
#include <klib/klog/klog.h>
#include <lib/string/string.h>

void cpu_cpuid(u32 leaf, u32 subleaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx) {
  __asm__ volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(subleaf));
}

/*
 * ============================================================================
 * CPUID SUPPORT
 * ============================================================================
 */

bool cpu_supports_nx(void) {
  u32 eax, ebx, ecx, edx;
  cpu_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
  return (edx & (1 << 20)) != 0;
}

bool cpu_supports_syscall(void) {
  u32 eax, ebx, ecx, edx;
  cpu_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
  return (edx & (1 << 11)) != 0;
}

/*
 * ============================================================================
 * MSR ACCESS
 * ============================================================================
 */

u64 cpu_rdmsr(u32 msr) {
  u32 lo, hi;
  __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
  return ((u64)hi << 32) | lo;
}

void cpu_wrmsr(u32 msr, u64 value) {
  u32 lo = (u32)(value & 0xFFFFFFFF);
  u32 hi = (u32)(value >> 32);
  __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

/*
 * ============================================================================
 * CR3 / TLB
 * ============================================================================
 */

u64 cpu_read_cr3(void) {
  u64 value;
  __asm__ volatile("mov %%cr3, %0" : "=r"(value));
  return value;
}

void cpu_write_cr3(u64 cr3_val) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(cr3_val));
}

void cpu_invlpg(u64 virt_addr) {
  __asm__ volatile("invlpg (%0)" : : "r"(virt_addr) : "memory");
}

/*
 * ============================================================================
 * EFER / NXE ENABLE
 * ============================================================================
 */

void cpu_enable_nxe_bit(void) {
  u64 efer = cpu_rdmsr(0xC0000080); // MSR_EFER
  if (!(efer & (1ULL << 11))) {
    efer |= (1ULL << 11);
    cpu_wrmsr(0xC0000080, efer);
    klog_info("cpu: Bit NXE abilitato in EFER");
  } else {
    klog_info("cpu: Bit NXE già attivo");
  }
}

/*
 * ============================================================================
 * INTERRUPT CONTROL
 * ============================================================================
 */

void cpu_halt(void) {
  __asm__ volatile("hlt");
}

void cpu_enable_interrupts(void) {
  __asm__ volatile("sti");
}

void cpu_disable_interrupts(void) {
  __asm__ volatile("cli");
}

/*
 * ============================================================================
 * IDENTIFICAZIONE CPU
 * ============================================================================
 */

const char *cpu_get_arch_name(void) {
  return "x86_64";
}

const char *cpu_get_vendor(void) {
  static char vendor[13] = {0};
  u32 eax, ebx, ecx, edx;
  cpu_cpuid(0x0, 0, &eax, &ebx, &ecx, &edx);
  *(u32 *)&vendor[0] = ebx;
  *(u32 *)&vendor[4] = edx;
  *(u32 *)&vendor[8] = ecx;
  return vendor;
}
/*
 * ============================================================================
 * CR2 READ
 * ============================================================================
 */
u64 cpu_read_cr2(void) {
  u64 val;
  asm volatile("mov %%cr2, %0" : "=r"(val));
  return val;
}
