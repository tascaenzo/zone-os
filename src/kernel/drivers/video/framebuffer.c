#include <drivers/video/framebuffer.h>
#include <lib/stdint.h>

// Puntatore alla base del framebuffer (memoria video fornita da Limine)
static uint8_t *framebuffer_address = 0;

// Dimensioni in pixel del framebuffer
static uint64_t framebuffer_width = 0;
static uint64_t framebuffer_height = 0;

// Numero di byte per riga (attenzione: può essere superiore a width * bytes_per_pixel)
static uint64_t framebuffer_pitch = 0;

// Bit per pixel (normalmente 32 - formato BGRA)
static uint16_t framebuffer_bpp = 0;

/**
 * @brief Inizializza il framebuffer con i parametri forniti dal bootloader.
 *
 * @param addr   Indirizzo base del framebuffer
 * @param width  Larghezza in pixel
 * @param height Altezza in pixel
 * @param pitch  Byte per riga
 * @param bpp    Bit per pixel (supportato: 32)
 */
void framebuffer_init(void *addr, uint64_t width, uint64_t height, uint64_t pitch, uint16_t bpp) {
  framebuffer_address = (uint8_t *)addr;
  framebuffer_width = width;
  framebuffer_height = height;
  framebuffer_pitch = pitch;
  framebuffer_bpp = bpp;
}

/**
 * @brief Disegna un singolo pixel in (x, y) con un colore BGRA 32-bit.
 *
 * @param x     Coordinata orizzontale
 * @param y     Coordinata verticale
 * @param color Colore nel formato 0xAABBGGRR (BGRA)
 */
void framebuffer_draw_pixel(uint64_t x, uint64_t y, uint32_t color) {
  if (x >= framebuffer_width || y >= framebuffer_height) {
    return;
  }

  // Calcolo dell'indirizzo del pixel (riga + colonna)
  uint8_t *pixel = framebuffer_address + y * framebuffer_pitch + x * (framebuffer_bpp / 8);
  *(uint32_t *)pixel = color;
}

/**
 * @brief Pulisce l'intero framebuffer con un colore uniforme.
 *
 * @param color Colore da usare per il riempimento
 */
void framebuffer_clear(uint32_t color) {
  for (uint64_t y = 0; y < framebuffer_height; ++y) {
    for (uint64_t x = 0; x < framebuffer_width; ++x) {
      framebuffer_draw_pixel(x, y, color);
    }
  }
}

/**
 * @brief Riempie un rettangolo dell'area video con un colore.
 *
 * @param x      Coordinata X iniziale
 * @param y      Coordinata Y iniziale
 * @param width  Larghezza del rettangolo in pixel
 * @param height Altezza del rettangolo in pixel
 * @param color  Colore da applicare (BGRA)
 */
void framebuffer_fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color) {
  for (uint64_t dy = 0; dy < height; ++dy) {
    for (uint64_t dx = 0; dx < width; ++dx) {
      framebuffer_draw_pixel(x + dx, y + dy, color);
    }
  }
}

/**
 * @brief Restituisce la larghezza del framebuffer.
 *
 * @return Numero di pixel orizzontali
 */
uint64_t framebuffer_get_width(void) {
  return framebuffer_width;
}

/**
 * @brief Restituisce l'altezza del framebuffer.
 *
 * @return Numero di pixel verticali
 */
uint64_t framebuffer_get_height(void) {
  return framebuffer_height;
}

/**
 * @brief Restituisce i bit per pixel (normalmente 32).
 */
uint16_t framebuffer_get_bpp(void) {
  return framebuffer_bpp;
}

/**
 * @brief Restituisce il puntatore alla memoria del framebuffer.
 */
void *framebuffer_get_address(void) {
  return framebuffer_address;
}
