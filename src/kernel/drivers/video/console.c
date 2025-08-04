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
  framebuffer_fill_rect(screen_x, screen_y, FONT_WIDTH, FONT_HEIGHT, bg_color);

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
  // ========================================================================
  // STEP 1: VALIDAZIONE PRELIMINARE E RACCOLTA PARAMETRI FRAMEBUFFER
  // ========================================================================

  uint64_t fb_width = framebuffer_get_width();
  uint64_t fb_height = framebuffer_get_height();
  uint16_t fb_bpp = framebuffer_get_bpp();
  uint8_t *fb = (uint8_t *)framebuffer_get_address();

  // Validazione parametri framebuffer - SILENT FAIL per evitare dipendenze circolari
  if (!fb) {
    return; // Framebuffer non disponibile
  }

  if (fb_width == 0 || fb_height == 0) {
    return; // Dimensioni framebuffer non valide
  }

  if (fb_bpp != 32) { // Assumiamo 32bpp come da design
    return;           // BPP non supportato
  }

  // Calcolo pitch con validazione overflow - SILENT FAIL
  uint64_t bytes_per_pixel = fb_bpp / 8;
  if (fb_width > UINT64_MAX / bytes_per_pixel) {
    return; // Overflow nel calcolo pitch
  }
  uint64_t pitch = fb_width * bytes_per_pixel;

  // ========================================================================
  // STEP 2: VALIDAZIONE LAYOUT CONSOLE
  // ========================================================================

  // Verifica che la console sia stata inizializzata - SILENT FAIL
  if (rows == 0 || cols == 0) {
    return; // Console non inizializzata
  }

  // Verifica che i border siano sensati - SILENT FAIL
  uint64_t total_text_width = BORDER_LEFT + (cols * CHAR_WIDTH) + BORDER_RIGHT;
  uint64_t total_text_height = BORDER_TOP + (rows * CHAR_HEIGHT) + BORDER_BOTTOM;

  if (total_text_width > fb_width || total_text_height > fb_height) {
    return; // Layout console incompatibile con framebuffer
  }

  // ========================================================================
  // STEP 3: CALCOLI SICURI DELLE DIMENSIONI
  // ========================================================================

  uint64_t text_start_y = BORDER_TOP;
  uint64_t text_width_bytes = cols * FONT_WIDTH * bytes_per_pixel;
  uint64_t text_area_height = (rows - 1) * FONT_HEIGHT;

  // Validazione text_width_bytes contro overflow - SILENT FAIL
  if (cols > UINT64_MAX / CHAR_WIDTH || (cols * CHAR_WIDTH) > UINT64_MAX / bytes_per_pixel) {
    return; // Overflow nel calcolo text_width_bytes
  }

  // Verifica che text_width_bytes non ecceda la larghezza disponibile
  uint64_t available_width_bytes = (fb_width - BORDER_LEFT - BORDER_RIGHT) * bytes_per_pixel;
  if (text_width_bytes > available_width_bytes) {
    return; // text_width_bytes eccede spazio disponibile
  }

  // ========================================================================
  // STEP 4: CALCOLO BOUNDS SICURO PER OGNI RIGA
  // ========================================================================

  uint8_t *fb_end = fb + (fb_height * pitch); // Primo byte dopo il framebuffer

  // Pre-calcola offset per evitare calcoli ripetuti nel loop
  uint64_t border_offset_bytes = BORDER_LEFT * bytes_per_pixel;

  // Verifica che tutti gli accessi siano nei bounds prima di iniziare
  for (uint64_t y = 0; y < text_area_height; ++y) {
    // Calcolo src address
    uint64_t src_y = text_start_y + y + CHAR_HEIGHT;
    if (src_y >= fb_height) {
      return; // src_y fuori bounds framebuffer height
    }

    // Calcolo dst address
    uint64_t dst_y = text_start_y + y;
    if (dst_y >= fb_height) {
      return; // dst_y fuori bounds framebuffer height
    }

    // Verifica overflow nei calcoli degli offset
    if (src_y > UINT64_MAX / pitch || dst_y > UINT64_MAX / pitch) {
      return; // Overflow nel calcolo offset
    }

    uint8_t *src = fb + src_y * pitch + border_offset_bytes;
    uint8_t *dst = fb + dst_y * pitch + border_offset_bytes;

    // Bounds check finale: verifica che src e dst + text_width_bytes siano nei bounds
    if (src < fb || src >= fb_end || (src + text_width_bytes) > fb_end) {
      return; // src address fuori bounds
    }

    if (dst < fb || dst >= fb_end || (dst + text_width_bytes) > fb_end) {
      return; // dst address fuori bounds
    }
  }

  // ========================================================================
  // STEP 5: ESECUZIONE SCROLL (ORA SICURA)
  // ========================================================================

  // Scrolla solo l'area di testo (ora sappiamo che è sicuro)
  for (uint64_t y = 0; y < text_area_height; ++y) {
    uint64_t src_y = text_start_y + y + CHAR_HEIGHT;
    uint64_t dst_y = text_start_y + y;

    uint8_t *src = fb + src_y * pitch + border_offset_bytes;
    uint8_t *dst = fb + dst_y * pitch + border_offset_bytes;

    // Ora possiamo fare memcpy in sicurezza
    memcpy(dst, src, text_width_bytes);
  }

  // ========================================================================
  // STEP 6: PULIZIA ULTIMA RIGA CON VALIDAZIONE
  // ========================================================================

  // Calcola posizione ultima riga
  uint64_t last_row_x = BORDER_LEFT;
  uint64_t last_row_y = BORDER_TOP + (rows - 1) * CHAR_HEIGHT;
  uint64_t last_row_width = cols * CHAR_WIDTH;
  uint64_t last_row_height = CHAR_HEIGHT;

  // Validazione finale per framebuffer_fill_rect - SILENT FAIL
  if (last_row_x + last_row_width > fb_width) {
    return; // Ultima riga eccede width framebuffer
  }

  if (last_row_y + last_row_height > fb_height) {
    return; // Ultima riga eccede height framebuffer
  }

  // Pulisce ultima riga nell'area di testo (ora sicura)
  framebuffer_fill_rect(last_row_x, last_row_y, last_row_width, last_row_height, bg_color);
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
