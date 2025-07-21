#include <drivers/video/console.h>
#include <drivers/video/font8x16.h>
#include <drivers/video/framebuffer.h>
#include <lib/stdint.h>
#include <lib/string.h>

// === Costanti per configurazione del font e layout ===
#define FONT_SCALE 1
#define FONT_BASE_WIDTH 8
#define FONT_BASE_HEIGHT 16
#define SPACING_H 0
#define SPACING_V 4

#define BORDER_LEFT 16
#define BORDER_TOP 16
#define BORDER_RIGHT 16
#define BORDER_BOTTOM 16

#define FONT_WIDTH (FONT_BASE_WIDTH * FONT_SCALE)
#define FONT_HEIGHT (FONT_BASE_HEIGHT * FONT_SCALE)
#define CHAR_WIDTH (FONT_WIDTH + SPACING_H)
#define CHAR_HEIGHT (FONT_HEIGHT + SPACING_V)

static uint64_t cols;
static uint64_t rows;

static uint64_t cursor_x = 0;
static uint64_t cursor_y = 0;

static uint32_t fg_color = CONSOLE_DEFAULT_FG;
static uint32_t bg_color = CONSOLE_DEFAULT_BG;

/**
 * @brief Disegna un carattere alla posizione (x, y) nella console.
 *        Il carattere è scalato secondo FONT_SCALE e viene disegnato usando il font bitmap.
 */
static void draw_char(uint64_t x, uint64_t y, char c) {
  if ((unsigned char)c >= 128)
    c = '?';
  const uint8_t *glyph = font8x16_basic[(uint8_t)c];

  // Calcola posizione con bordatura
  uint64_t screen_x = BORDER_LEFT + x;
  uint64_t screen_y = BORDER_TOP + y;

  // Pulisce l'area del carattere (include spaziatura)
  framebuffer_fill_rect(screen_x, screen_y, CHAR_WIDTH, CHAR_HEIGHT, bg_color);

  // Disegna il carattere scalato
  for (uint8_t row = 0; row < FONT_BASE_HEIGHT; ++row) {
    uint8_t line = glyph[row];
    for (uint8_t col = 0; col < FONT_BASE_WIDTH; ++col) {
      if (line & (1 << (7 - col))) {
        // Disegna un blocco di dimensione FONT_SCALE x FONT_SCALE
        for (uint8_t sy = 0; sy < FONT_SCALE; ++sy) {
          for (uint8_t sx = 0; sx < FONT_SCALE; ++sx) {
            uint64_t px = screen_x + (col * FONT_SCALE) + sx + (SPACING_H / 2);
            uint64_t py = screen_y + (row * FONT_SCALE) + sy + (SPACING_V / 2);
            framebuffer_draw_pixel(px, py, fg_color);
          }
        }
      }
    }
  }
}

/**
 * @brief Esegue lo scroll del contenuto della console verso l'alto di una riga.
 *        Sposta la memoria framebuffer e pulisce l'ultima riga.
 */
static void console_scroll() {
  uint64_t pitch = framebuffer_get_width() * (framebuffer_get_bpp() / 8);
  uint8_t *fb = (uint8_t *)framebuffer_get_address();

  // Area di testo (escludendo bordatura)
  uint64_t text_start_y = BORDER_TOP;

  // Scrolla solo l'area di testo
  for (uint64_t y = 0; y < (rows - 1) * CHAR_HEIGHT; ++y) {
    uint8_t *src = fb + (text_start_y + y + CHAR_HEIGHT) * pitch + BORDER_LEFT * (framebuffer_get_bpp() / 8);
    uint8_t *dst = fb + (text_start_y + y) * pitch + BORDER_LEFT * (framebuffer_get_bpp() / 8);
    uint64_t text_width_bytes = cols * CHAR_WIDTH * (framebuffer_get_bpp() / 8);
    memcpy(dst, src, text_width_bytes);
  }

  // Pulisce ultima riga nell'area di testo
  framebuffer_fill_rect(BORDER_LEFT, BORDER_TOP + (rows - 1) * CHAR_HEIGHT, cols * CHAR_WIDTH, CHAR_HEIGHT, bg_color);
}

/**
 * @brief Inizializza la console framebuffer, calcolando le dimensioni della griglia caratteri.
 */
void console_init(void) {
  framebuffer_clear(bg_color);
  cursor_x = 0;
  cursor_y = 0;

  // Calcola righe e colonne considerando la bordatura
  uint64_t usable_width = framebuffer_get_width() - BORDER_LEFT - BORDER_RIGHT;
  uint64_t usable_height = framebuffer_get_height() - BORDER_TOP - BORDER_BOTTOM;

  cols = usable_width / CHAR_WIDTH;
  rows = usable_height / CHAR_HEIGHT;
}

/**
 * @brief Stampa un carattere nella posizione corrente del cursore.
 *        Gestisce anche newline e backspace. Scrolla se necessario.
 */
void console_putc(char c) {
  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
  } else if (c == '\b') {
    if (cursor_x > 0)
      cursor_x--;
    draw_char(cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT, ' ');
  } else {
    draw_char(cursor_x * CHAR_WIDTH, cursor_y * CHAR_HEIGHT, c);
    cursor_x++;
    if (cursor_x >= cols) {
      cursor_x = 0;
      cursor_y++;
    }
  }

  if (cursor_y >= rows) {
    console_scroll();
    cursor_y = rows - 1;
  }
}

/**
 * @brief Stampa una stringa intera sulla console.
 */
void console_write(const char *str) {
  while (*str) {
    console_putc(*str++);
  }
}

/**
 * @brief Cancella tutto lo schermo e resetta la posizione del cursore.
 */
void console_clear(void) {
  framebuffer_clear(bg_color);
  cursor_x = 0;
  cursor_y = 0;
}

/**
 * @brief Imposta il colore del testo e dello sfondo.
 */
void console_set_color(uint32_t fg, uint32_t bg) {
  fg_color = fg;
  bg_color = bg;
}

/**
 * @brief Ritorna i colori correnti (testo e sfondo).
 */
void console_get_color(uint32_t *fg, uint32_t *bg) {
  if (fg)
    *fg = fg_color;
  if (bg)
    *bg = bg_color;
}

/**
 * @brief Imposta solo il colore del testo.
 */
void console_set_fg_color(uint32_t fg) {
  fg_color = fg;
}

/**
 * @brief Imposta solo il colore dello sfondo.
 */
void console_set_bg_color(uint32_t bg) {
  bg_color = bg;
}

/**
 * @brief Ripristina i colori predefiniti della console.
 */
void console_reset_colors(void) {
  fg_color = CONSOLE_DEFAULT_FG;
  bg_color = CONSOLE_DEFAULT_BG;
}

/**
 * @brief Imposta la posizione del cursore.
 */
void console_set_cursor(uint16_t row, uint16_t col) {
  if (row < rows)
    cursor_y = row;
  if (col < cols)
    cursor_x = col;
}

/**
 * @brief Ottiene la posizione corrente del cursore.
 */
void console_get_cursor(uint16_t *row, uint16_t *col) {
  if (row)
    *row = (uint16_t)cursor_y;
  if (col)
    *col = (uint16_t)cursor_x;
}
