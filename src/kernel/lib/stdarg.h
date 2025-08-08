#pragma once

/**
 * @file stdarg.h
 * @brief Support per funzioni con argomenti variabili (variadic functions)
 *
 * Implementazione basata su GCC/Clang built-ins per massima compatibilità
 * e performance nel kernel.
 */

/**
 * @brief Tipo opaco per contenere la lista di argomenti variabili
 */
typedef __builtin_va_list va_list;

/**
 * @brief Inizializza va_list per accedere agli argomenti variabili
 * @param v     Lista argomenti (va_list)
 * @param last  Ultimo parametro fisso prima di "..."
 *
 * Esempio: per kprintf(const char* format, ...)
 *          va_start(args, format) inizializza per leggere dopo 'format'
 */
#define va_start(v, last) __builtin_va_start(v, last)

/**
 * @brief Pulisce e finalizza va_list dopo l'uso
 * @param v Lista argomenti da pulire
 *
 * IMPORTANTE: Chiamare sempre dopo va_start() per evitare memory leak
 */
#define va_end(v) __builtin_va_end(v)

/**
 * @brief Legge il prossimo argomento dalla lista
 * @param v    Lista argomenti
 * @param type Tipo dell'argomento da leggere (int, char*, etc.)
 * @return     Valore dell'argomento del tipo specificato
 *
 * Esempio: int num = va_arg(args, int);
 *          char* str = va_arg(args, char*);
 *
 * ATTENZIONE: Il tipo deve corrispondere esattamente a quello passato!
 */
#define va_arg(v, type) __builtin_va_arg(v, type)

/**
 * @brief Copia una va_list in un'altra (utile per implementazioni interne)
 * @param dest Destinazione
 * @param src  Sorgente
 */
#define va_copy(dest, src) __builtin_va_copy(dest, src)