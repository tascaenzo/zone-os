#ifndef PRINT_H
#define PRINT_H

#include <stdint.h>

void put_char(uint32_t *fb, uint64_t pitch, uint64_t fb_width, uint64_t x, uint64_t y, char c, uint32_t color);
void put_string(uint32_t *fb, uint64_t pitch, uint64_t fb_width, uint64_t x, uint64_t y, const char *s, uint32_t color);
void init_print(void);

#endif
