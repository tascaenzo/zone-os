#pragma once

#include <lib/stddef.h>
#include <lib/stdint.h>

/**
 * @brief Copia n byte da src a dest (non sicuro per overlap).
 * @return Puntatore a dest.
 */
void *memcpy(void *dest, const void *src, size_t n);

/**
 * @brief Copia n byte da src a dest gestendo sovrapposizioni.
 * @return Puntatore a dest.
 */
void *memmove(void *dest, const void *src, size_t n);

/**
 * @brief Imposta n byte della memoria a un valore specifico.
 * @return Puntatore a dest.
 */
void *memset(void *dest, int value, size_t n);

/**
 * @brief Confronta n byte tra due buffer.
 * @return 0 se uguali, valore negativo/positivo al primo mismatch.
 */
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * @brief Calcola la lunghezza di una stringa (fino al terminatore).
 * @return Numero di caratteri (escluso '\0').
 */
size_t strlen(const char *str);

/**
 * @brief Confronta due stringhe ASCII.
 * @return 0 se uguali, valore negativo/positivo se s1<s2 o s1>s2.
 */
int strcmp(const char *s1, const char *s2);

/**
 * @brief Confronta al massimo n caratteri tra due stringhe.
 * @return 0 se uguali, valore negativo/positivo se diversi.
 */
int strncmp(const char *s1, const char *s2, size_t n);

/**
 * @brief Copia una stringa terminata da '\0' in dest.
 * @return Puntatore a dest.
 */
char *strcpy(char *dest, const char *src);

/**
 * @brief Copia al massimo n caratteri da src a dest, con padding '\0'.
 * @return Puntatore a dest.
 */
char *strncpy(char *dest, const char *src, size_t n);
