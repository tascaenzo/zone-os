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

// === Gestione Colori ===

/**
 * @brief Imposta i colori della console (foreground e background)
 * @param fg_color Colore del testo (formato ARGB: 0xAARRGGBB)
 * @param bg_color Colore dello sfondo (formato ARGB: 0xAARRGGBB)
 */
void console_set_color(uint32_t fg_color, uint32_t bg_color);

/**
 * @brief Ottiene i colori correnti della console
 * @param fg_color Puntatore per salvare il colore del testo (può essere NULL)
 * @param bg_color Puntatore per salvare il colore dello sfondo (può essere NULL)
 */
void console_get_color(uint32_t *fg_color, uint32_t *bg_color);

/**
 * @brief Imposta solo il colore del testo, mantenendo lo sfondo
 * @param fg_color Nuovo colore del testo
 */
void console_set_fg_color(uint32_t fg_color);

/**
 * @brief Imposta solo il colore dello sfondo, mantenendo il testo
 * @param bg_color Nuovo colore dello sfondo
 */
void console_set_bg_color(uint32_t bg_color);

/**
 * @brief Ripristina i colori di default della console
 */
void console_reset_colors(void);

// === Gestione Cursore ===

/**
 * @brief Imposta la posizione del cursore
 * @param row Riga (0-based)
 * @param col Colonna (0-based)
 */
void console_set_cursor(uint16_t row, uint16_t col);

/**
 * @brief Ottiene la posizione corrente del cursore
 * @param row Puntatore per salvare la riga (può essere NULL)
 * @param col Puntatore per salvare la colonna (può essere NULL)
 */
void console_get_cursor(uint16_t *row, uint16_t *col);

// === Colori Predefiniti ===
#define CONSOLE_COLOR_BLACK 0xFF000000
#define CONSOLE_COLOR_DARK_BLUE 0xFF000080
#define CONSOLE_COLOR_DARK_GREEN 0xFF008000
#define CONSOLE_COLOR_DARK_CYAN 0xFF008080
#define CONSOLE_COLOR_DARK_RED 0xFF800000
#define CONSOLE_COLOR_DARK_MAGENTA 0xFF800080
#define CONSOLE_COLOR_BROWN 0xFF808000
#define CONSOLE_COLOR_LIGHT_GREY 0xFFC0C0C0
#define CONSOLE_COLOR_DARK_GREY 0xFF808080
#define CONSOLE_COLOR_BLUE 0xFF0000FF
#define CONSOLE_COLOR_GREEN 0xFF00FF00
#define CONSOLE_COLOR_CYAN 0xFF00FFFF
#define CONSOLE_COLOR_RED 0xFFFF0000
#define CONSOLE_COLOR_MAGENTA 0xFFFF00FF
#define CONSOLE_COLOR_YELLOW 0xFFFFFF00
#define CONSOLE_COLOR_WHITE 0xFFFFFFFF

// Colori di default del sistema
#define CONSOLE_DEFAULT_FG 0xFFE0E0E0 // Grigio chiaro
#define CONSOLE_DEFAULT_BG 0xFF101010 // Grigio scuro