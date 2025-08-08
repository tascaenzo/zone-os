#pragma once

#include "stdbool.h" // Per bool
#include "stddef.h"  // Per size_t / ptrdiff_t
#include "stdint.h"  // Per uintN_t / intN_t

// Tipi unsigned abbreviati
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Tipi signed abbreviati
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

// Alias per size_t e ptrdiff_t
typedef size_t usize;    // dimensioni/indici
typedef ptrdiff_t isize; // differenza tra puntatori

// Tipi per puntatori come interi
typedef u64 uptr; // puntatore unsigned (indirizzo come numero)
typedef s64 iptr; // puntatore signed (per aritmetica con offset)

// Tipi fisici per indirizzi
typedef u64 phys_addr_t;
typedef u64 virt_addr_t;