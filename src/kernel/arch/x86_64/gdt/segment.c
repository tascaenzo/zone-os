 /**
 * @file arch/x86_64/gdt/segment.c
 * @brief API di inizializzazione della segmentazione per ZONE-OS
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include <arch/segment.h>

/* Header privato del backend GDT/TSS */
#include "gdt.h"

#include <klib/klog/klog.h>

void arch_segment_init(void) {
  /* Inizializza e carica GDT + TSS (lgdt/ltr/far-ret inside) */
  gdt_init();

  /* Log dopo il load: ora i registri segmento sono coerenti */
  klog_info("arch[x86_64]: segment initialized (GDT/TSS)\n");
}
