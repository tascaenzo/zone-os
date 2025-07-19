#pragma once
#include <lib/stdint.h>

/**
 * @brief Inizializza la console framebuffer (font, stato, colori, cursore)
 */
void console_init(void);

/**
 * @brief Scrive un carattere alla posizione corrente, gestisce newline e scroll
 * @param c Carattere ASCII
 */
void console_putc(char c);

/**
 * @brief Scrive una stringa intera (terminata da \0)
 * @param str Puntatore alla stringa
 */
void console_write(const char *str);

/**
 * @brief Cancella tutto il contenuto della console
 */
void console_clear(void);
