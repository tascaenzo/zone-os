#pragma once

/*
 * stdbool.h - Definizione dei tipi booleani in C per ambienti freestanding.
 *
 * Lo standard C99 introduce il tipo _Bool come tipo built-in,
 * ma fornisce una sintassi più leggibile usando 'bool'.
 *
 * In ambiente kernel non si include lo stdlib del compilatore,
 * quindi definiamo tutto da zero.
 */

// Se il compilatore supporta _Bool (C99+), usiamolo direttamente
#ifndef __cplusplus
typedef _Bool bool;
#define true 1
#define false 0
#endif

// Macro usata per verificare se stdbool.h è stato incluso
#define __bool_true_false_are_defined 1
