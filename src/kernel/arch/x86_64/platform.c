/**
 * @file arch/x86_64/platform.c
 * @brief Platform layer implementation (x86_64) for ZONE-OS
 *
 * Questo file implementa le API portabili dichiarate in <arch/platform.h>
 * fornendo il nome della piattaforma e il punto di inizializzazione
 * architetturale. L’inizializzazione dettagliata (GDT, IDT, timer, IRQ, ecc.)
 * verrà aggiunta progressivamente; per ora è un placeholder minimo.
 *
 * Funzionalità coperte (baseline):
 *  - Identificazione piattaforma (arch_get_name)
 *  - Entry di init architetturale (arch_init)
 *
 * @author Enzo Tasca
 * @date 2025
 */

#include <arch/platform.h>
#include <klib/klog/klog.h>
#include <lib/types.h>
#include <limine.h>

const char *arch_get_name(void) {
  return "x86_64";
}

void arch_init(void) {

  return;
}
