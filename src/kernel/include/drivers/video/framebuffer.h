#pragma once
#include <lib/stdint.h>

/**
 * @brief Inizializza il modulo framebuffer in modo agnostico all'hardware.
 * Deve essere chiamata una sola volta a boot completato.
 *
 * @param addr   Indirizzo base del framebuffer in memoria (fornito da Limine)
 * @param width  Larghezza del framebuffer in pixel
 * @param height Altezza del framebuffer in pixel
 * @param pitch  Byte per riga (di solito >= width * (bpp / 8))
 * @param bpp    Bit per pixel (supportato 32 bpp)
 */
void framebuffer_init(void *addr, uint64_t width, uint64_t height, uint64_t pitch, uint16_t bpp);

/**
 * @brief Disegna un pixel nella posizione (x, y) con un colore in formato BGRA.
 *
 * @param x     Coordinata orizzontale (in pixel)
 * @param y     Coordinata verticale (in pixel)
 * @param color Valore del colore nel formato 0xAABBGGRR
 */
void framebuffer_draw_pixel(uint64_t x, uint64_t y, uint32_t color);

/**
 * @brief Riempie tutto lo schermo con il colore specificato.
 *
 * @param color Colore nel formato BGRA (0xAABBGGRR)
 */
void framebuffer_clear(uint32_t color);

/**
 * @brief Riempie un rettangolo con il colore specificato.
 *
 * @param x      Coordinata X iniziale
 * @param y      Coordinata Y iniziale
 * @param width  Larghezza del rettangolo in pixel
 * @param height Altezza del rettangolo in pixel
 * @param color  Colore da utilizzare (BGRA)
 */
void framebuffer_fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color);

/**
 * @brief Restituisce la larghezza del framebuffer in pixel.
 */
uint64_t framebuffer_get_width(void);

/**
 * @brief Restituisce l'altezza del framebuffer in pixel.
 */
uint64_t framebuffer_get_height(void);

/**
 * @brief Restituisce il numero di bit per pixel (es. 32).
 */
uint16_t framebuffer_get_bpp(void);

/**
 * @brief Restituisce l'indirizzo base del framebuffer.
 */
void *framebuffer_get_address(void);
