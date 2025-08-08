#pragma once

#include <lib/stdarg.h>
#include <lib/types.h>

/**
 * @brief Printf per kernel - output formattato su console
 * @param format String di formato con specifiers (%d, %s, %x, etc.)
 * @return Numero di caratteri stampati, -1 in caso di errore
 */
int kprintf(const char *format, ...);

/**
 * @brief Printf in buffer - formattazione in memoria
 * @param buffer Buffer di destinazione
 * @param format String di formato
 * @return Numero di caratteri scritti nel buffer
 */
int ksprintf(char *buffer, const char *format, ...);

/**
 * @brief Printf sicuro in buffer con limite dimensione
 * @param buffer Buffer di destinazione
 * @param size Dimensione massima buffer (include '\0')
 * @param format String di formato
 * @return Numero di caratteri che sarebbero stati scritti
 */
int ksnprintf(char *buffer, size_t size, const char *format, ...);

/**
 * @brief Printf con va_list - per implementazioni interne
 * @param format String di formato
 * @param args Lista argomenti variabili
 * @return Numero di caratteri stampati
 */
int kvprintf(const char *format, va_list args);

/**
 * @brief Stampa singolo carattere su console kernel
 * @param c Carattere da stampare
 * @return Carattere stampato, EOF in caso di errore
 */
int kputchar(int c);

/**
 * @brief Stampa stringa su console kernel (con newline automatico)
 * @param s Stringa terminata da '\0'
 * @return Numero di caratteri stampati, EOF in caso di errore
 */
int kputs(const char *s);

// === Constants ===
#define EOF (-1)

// === Format specifiers supportati ===
// %d, %i  - signed decimal integer
// %u      - unsigned decimal integer
// %x, %X  - hexadecimal (lowercase/uppercase)
// %o      - octal
// %c      - single character
// %s      - string
// %p      - pointer address
// %%      - literal %