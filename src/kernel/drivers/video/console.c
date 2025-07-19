#include <drivers/video/console.h>
#include <drivers/video/font8x16.h> // Cambiato da font8x8.h
#include <drivers/video/framebuffer.h>
#include <lib/stdint.h>
#include <lib/string.h>

// === CONFIGURAZIONE AUTOMATICA ===
#define FONT_SCALE 1        // Cambia questo per scalare tutto automaticamente! (1, 2, 3, 4...)
#define FONT_BASE_WIDTH 8   // Dimensione base del font
#define FONT_BASE_HEIGHT 16 // Dimensione base del font (era 8, ora 16)
#define SPACING_H 0         // Spaziatura orizzontale extra (pixel)
#define SPACING_V 4         // Spaziatura verticale extra (pixel)

// === BORDATURA CONSOLE ===
#define BORDER_LEFT 16   // Pixel di margine sinistro
#define BORDER_TOP 16    // Pixel di margine superiore
#define BORDER_RIGHT 16  // Pixel di margine destro
#define BORDER_BOTTOM 16 // Pixel di margine inferiore

// Calcoli automatici (non toccare)
#define FONT_WIDTH (FONT_BASE_WIDTH * FONT_SCALE)
#define FONT_HEIGHT (FONT_BASE_HEIGHT * FONT_SCALE)
#define CHAR_WIDTH (FONT_WIDTH + SPACING_H)
#define CHAR_HEIGHT (FONT_HEIGHT + SPACING_V)

static uint64_t cols;
static uint64_t rows;

static uint64_t cursor_x = 0;
static uint64_t cursor_y = 0;

static uint32_t fg_color = 0xFFE0E0E0; // Grigio chiaro
static uint32_t bg_color = 0xFF101010; // Grigio scuro

// === Funzione privata: disegna un carattere scalato automaticamente ===
static void draw_char(uint64_t x, uint64_t y, char c) {
  if ((unsigned char)c >= 128)
    c = '?';
  const uint8_t *glyph = font8x16_basic[(uint8_t)c]; // Cambiato da font8x8_basic

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

// === Scroll console in alto di 1 riga ===
static void console_scroll() {
  uint64_t pitch = framebuffer_get_width() * (framebuffer_get_bpp() / 8);
  uint8_t *fb = (uint8_t *)framebuffer_get_address();

  // Area di testo (escludendo bordatura)
  uint64_t text_start_y = BORDER_TOP;

  // Scrolla solo l'area di testo
  for (uint64_t y = 0; y < (rows - 1) * CHAR_HEIGHT; ++y) {
    uint8_t *src = fb + (text_start_y + y + CHAR_HEIGHT) * pitch + BORDER_LEFT * (framebuffer_get_bpp() / 8);
    uint8_t *dst = fb + (text_start_y + y) * pitch + BORDER_LEFT * (framebuffer_get_bpp() / 8);

    // Copia solo la larghezza dell'area di testo
    uint64_t text_width_bytes = cols * CHAR_WIDTH * (framebuffer_get_bpp() / 8);
    memcpy(dst, src, text_width_bytes);
  }

  // Pulisce ultima riga nell'area di testo
  framebuffer_fill_rect(BORDER_LEFT, BORDER_TOP + (rows - 1) * CHAR_HEIGHT, cols * CHAR_WIDTH, CHAR_HEIGHT, bg_color);
}

// === Inizializza la console ===
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

// === Scrive un carattere, gestisce newline, scroll ===
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

// === Scrive una stringa intera ===
void console_write(const char *str) {
  while (*str) {
    console_putc(*str++);
  }
}

// === Cancella lo schermo e resetta il cursore ===
void console_clear(void) {
  framebuffer_clear(bg_color);
  cursor_x = 0;
  cursor_y = 0;
}