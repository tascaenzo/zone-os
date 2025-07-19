/* #include "limine.h"
#include <lib/stddef.h>
#include <lib/stdint.h>

// Richiesta framebuffer Limine
__attribute__((used)) static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

// Font 8x8 ASCII (solo caratteri A-Z a-z 0-9 e base)
static const uint8_t font[128][8] = {
    ['H'] = {0x42, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00},
    ['e'] = {0x00, 0x3C, 0x42, 0x7E, 0x40, 0x42, 0x3C, 0x00},
    ['l'] = {0x60, 0x20, 0x20, 0x20, 0x20, 0x22, 0x1C, 0x00},
    ['o'] = {0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00},
    ['W'] = {0x00, 0x42, 0x42, 0x5A, 0x5A, 0x66, 0x42, 0x00},
    ['r'] = {0x00, 0x5C, 0x62, 0x40, 0x40, 0x40, 0x40, 0x00},
    ['d'] = {0x0C, 0x04, 0x3C, 0x44, 0x44, 0x44, 0x3E, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

void put_char(uint32_t *fb, uint64_t pitch, uint64_t fb_width, uint64_t x, uint64_t y, char c,
              uint32_t color) {
  if (c < 0 || c > 127)
    return;
  for (int row = 0; row < 8; row++) {
    uint8_t line = font[(int)c][row];
    for (int col = 0; col < 8; col++) {
      if (line & (1 << (7 - col))) {
        fb[(y + row) * pitch / 4 + (x + col)] = color;
      }
    }
  }
}

void put_string(uint32_t *fb, uint64_t pitch, uint64_t fb_width, uint64_t x, uint64_t y,
                const char *s, uint32_t color) {
  while (*s) {
    put_char(fb, pitch, fb_width, x, y, *s++, color);
    x += 8;
  }
}

void init_print() {
  if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
    while (1)
      __asm__("hlt");
  }

  struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
  uint32_t *fb_ptr = (uint32_t *)fb->address;

  // Sfondo nero
  for (uint64_t y = 0; y < fb->height; y++) {
    for (uint64_t x = 0; x < fb->width; x++) {
      fb_ptr[y * fb->pitch / 4 + x] = 0x000000;
    }
  }

  // Stampa "Hello World"
  put_string(fb_ptr, fb->pitch, fb->width, 15, 15, "Hello World!!", 0xFFFFFF);
}
 */