#pragma once
#include <lib/types.h>

/**
 * @brief Scrive un byte (8 bit) su una porta I/O specifica (I/O Port Output)
 *
 * Utilizzato per inviare comandi o dati a dispositivi hardware mappati su porte I/O.
 *
 * @param port Porta I/O su cui scrivere
 * @param value Valore da scrivere (8 bit)
 */
static inline void outb(u16 port, u8 value) {
  __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
  // %0 = valore da scrivere (in registro AL)
  // %1 = numero di porta (in DX)
}

/**
 * @brief Legge un byte (8 bit) da una porta I/O specifica (I/O Port Input)
 *
 * Utilizzato per leggere dati da periferiche come tastiera, controller PIC, ecc.
 *
 * @param port Porta I/O da cui leggere
 * @return Byte letto dalla porta
 */
static inline u8 inb(u16 port) {
  u8 ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  // %1 = numero di porta (in DX), %0 = valore restituito (in AL)
  return ret;
}
