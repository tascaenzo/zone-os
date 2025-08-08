#include "string.h"
#include <lib/stddef.h>
#include <lib/stdint.h>

/**
 * @brief Copia n byte dalla sorgente alla destinazione (non gestisce overlap).
 */
void *memcpy(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return dest;
}

/**
 * @brief Imposta n byte della destinazione al valore specificato.
 */
void *memset(void *dest, int value, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  for (size_t i = 0; i < n; i++) {
    d[i] = (uint8_t)value;
  }
  return dest;
}

/**
 * @brief Copia n byte gestendo anche aree sovrapposte (sicura).
 */
void *memmove(void *dest, const void *src, size_t n) {
  uint8_t *d = (uint8_t *)dest;
  const uint8_t *s = (const uint8_t *)src;

  if (d < s) {
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else {
    for (size_t i = n; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }

  return dest;
}

/**
 * @brief Confronta n byte tra due buffer.
 *
 * @return 0 se uguali, valore positivo/negativo se diversi
 */
int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *a = (const uint8_t *)s1;
  const uint8_t *b = (const uint8_t *)s2;

  for (size_t i = 0; i < n; i++) {
    if (a[i] != b[i]) {
      return a[i] - b[i];
    }
  }

  return 0;
}

/**
 * @brief Calcola la lunghezza di una stringa (fino a '\0').
 */
size_t strlen(const char *str) {
  size_t len = 0;
  while (str[len]) {
    len++;
  }
  return len;
}

/**
 * @brief Confronta due stringhe fino al primo carattere diverso.
 *
 * @return 0 se uguali, valore positivo/negativo se diversi
 */
int strcmp(const char *s1, const char *s2) {
  while (*s1 && (*s1 == *s2)) {
    s1++;
    s2++;
  }
  return (uint8_t)(*s1) - (uint8_t)(*s2);
}

/**
 * @brief Confronta due stringhe fino a n caratteri.
 */
int strncmp(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i] || s1[i] == '\0' || s2[i] == '\0') {
      return (uint8_t)s1[i] - (uint8_t)s2[i];
    }
  }
  return 0;
}

/**
 * @brief Copia una stringa terminata da '\0' da src a dest.
 */
char *strcpy(char *dest, const char *src) {
  char *d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

/**
 * @brief Copia fino a n caratteri da src a dest, e aggiunge padding con '\0'.
 */
char *strncpy(char *dest, const char *src, size_t n) {
  size_t i = 0;

  for (; i < n && src[i]; i++) {
    dest[i] = src[i];
  }

  for (; i < n; i++) {
    dest[i] = '\0';
  }

  return dest;
}
