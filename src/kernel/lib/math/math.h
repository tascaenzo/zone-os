#pragma once

#include <lib/types.h>

/**
 * @file lib/math.h
 * @brief Libreria matematica minimalista per kernel
 *
 * Contiene solo le funzioni matematiche essenziali per lo sviluppo kernel:
 * - Aritmetica base per calcoli di sistema
 * - Bit manipulation per gestione hardware
 * - Allineamento e arrotondamento per memoria
 * - Utilità per algoritmi kernel
 */

/*
 * ============================================================================
 * BASIC ARITHMETIC
 * ============================================================================
 */

/**
 * @brief Valore assoluto
 */
__attribute__((unused)) static inline s32 math_abs(s32 x) {
  return x < 0 ? -x : x;
}

__attribute__((unused)) static inline s64 math_abs64(s64 x) {
  return x < 0 ? -x : x;
}

/**
 * @brief Min/max
 */
__attribute__((unused)) static inline s32 math_min(s32 a, s32 b) {
  return a < b ? a : b;
}

__attribute__((unused)) static inline s32 math_max(s32 a, s32 b) {
  return a > b ? a : b;
}

__attribute__((unused)) static inline u32 math_minu(u32 a, u32 b) {
  return a < b ? a : b;
}

__attribute__((unused)) static inline u32 math_maxu(u32 a, u32 b) {
  return a > b ? a : b;
}

__attribute__((unused)) static inline u64 math_minu64(u64 a, u64 b) {
  return a < b ? a : b;
}

__attribute__((unused)) static inline u64 math_maxu64(u64 a, u64 b) {
  return a > b ? a : b;
}

/**
 * @brief Clamp (limita valore in un range)
 */
__attribute__((unused)) static inline s32 math_clamp(s32 x, s32 min_val, s32 max_val) {
  if (x < min_val)
    return min_val;
  if (x > max_val)
    return max_val;
  return x;
}

__attribute__((unused)) static inline u64 math_clamp64(u64 x, u64 min_val, u64 max_val) {
  if (x < min_val)
    return min_val;
  if (x > max_val)
    return max_val;
  return x;
}

/*
 * ============================================================================
 * POWER AND ROOTS (solo integer per kernel)
 * ============================================================================
 */

/**
 * @brief Potenza intera (base^exp)
 */
u64 math_pow_int(u64 base, u32 exp);

/**
 * @brief Radice quadrata intera
 */
u32 math_sqrt_int(u64 x);

/*
 * ============================================================================
 * BIT MANIPULATION (essenziali per kernel)
 * ============================================================================
 */

/**
 * @brief Conta bit settati
 */
__attribute__((unused)) static inline u32 math_popcount(u32 x) {
  return __builtin_popcount(x);
}

__attribute__((unused)) static inline u32 math_popcount64(u64 x) {
  return __builtin_popcountll(x);
}

/**
 * @brief Leading zeros (da MSB)
 */
__attribute__((unused)) static inline u32 math_clz(u32 x) {
  return x ? __builtin_clz(x) : 32;
}

__attribute__((unused)) static inline u32 math_clz64(u64 x) {
  return x ? __builtin_clzll(x) : 64;
}

/**
 * @brief Trailing zeros (da LSB)
 */
__attribute__((unused)) static inline u32 math_ctz(u32 x) {
  return x ? __builtin_ctz(x) : 32;
}

__attribute__((unused)) static inline u32 math_ctz64(u64 x) {
  return x ? __builtin_ctzll(x) : 64;
}

/**
 * @brief Logaritmo base 2 intero
 */
__attribute__((unused)) static inline u32 math_log2_int(u32 x) {
  return x ? (31 - __builtin_clz(x)) : 0;
}

__attribute__((unused)) static inline u32 math_log2_int64(u64 x) {
  return x ? (63 - __builtin_clzll(x)) : 0;
}

/**
 * @brief Prossima potenza di 2
 */
__attribute__((unused)) static inline u32 math_next_pow2(u32 x) {
  if (x <= 1)
    return 1;
  return 1U << (32 - __builtin_clz(x - 1));
}

__attribute__((unused)) static inline u64 math_next_pow2_64(u64 x) {
  if (x <= 1)
    return 1;
  return 1ULL << (64 - __builtin_clzll(x - 1));
}

/**
 * @brief Verifica se è potenza di 2
 */
__attribute__((unused)) static inline bool math_is_pow2(u32 x) {
  return x && !(x & (x - 1));
}

__attribute__((unused)) static inline bool math_is_pow2_64(u64 x) {
  return x && !(x & (x - 1));
}

/*
 * ============================================================================
 * ALIGNMENT (cruciali per memoria kernel)
 * ============================================================================
 */

/**
 * @brief Allinea verso l'alto
 */
__attribute__((unused)) static inline u64 math_align_up(u64 value, u64 alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Allinea verso il basso
 */
__attribute__((unused)) static inline u64 math_align_down(u64 value, u64 alignment) {
  return value & ~(alignment - 1);
}

/**
 * @brief Verifica se è allineato
 */
__attribute__((unused)) static inline bool math_is_aligned(u64 value, u64 alignment) {
  return (value & (alignment - 1)) == 0;
}

/*
 * ============================================================================
 * DIVISION UTILITIES (per evitare divisioni costose)
 * ============================================================================
 */

/**
 * @brief Divisione con arrotondamento verso l'alto
 */
__attribute__((unused)) static inline u64 math_div_round_up(u64 dividend, u64 divisor) {
  return (dividend + divisor - 1) / divisor;
}

/**
 * @brief Modulo per potenze di 2 (più veloce)
 */
__attribute__((unused)) static inline u32 math_mod_pow2(u32 value, u32 pow2) {
  return value & (pow2 - 1);
}

__attribute__((unused)) static inline u64 math_mod_pow2_64(u64 value, u64 pow2) {
  return value & (pow2 - 1);
}

/*
 * ============================================================================
 * HASH FUNCTIONS (utili per kernel data structures)
 * ============================================================================
 */

/**
 * @brief Hash veloce per 32-bit
 */
__attribute__((unused)) static inline u32 math_hash32(u32 x) {
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = (x >> 16) ^ x;
  return x;
}

/**
 * @brief Hash veloce per 64-bit
 */
__attribute__((unused)) static inline u64 math_hash64(u64 x) {
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  x = x ^ (x >> 31);
  return x;
}

/*
 * ============================================================================
 * BASIC RANDOM (per testing e debug)
 * ============================================================================
 */

/**
 * @brief Semplice random per debug/testing
 */
void math_srand(u32 seed);
u32 math_rand(void);

#define MATH_RAND_MAX 0x7FFFFFFF

/*
 * ============================================================================
 * GREATEST COMMON DIVISOR (per algoritmi di scheduling)
 * ============================================================================
 */

/**
 * @brief Massimo comun divisore
 */
u64 math_gcd(u64 a, u64 b);

/**
 * @brief Minimo comune multiplo
 */
u64 math_lcm(u64 a, u64 b);

/*
 * ============================================================================
 * SIZE CONVERSIONS (utili per memoria)
 * ============================================================================
 */

// Costanti per conversioni dimensioni
#define MATH_KB 1024UL
#define MATH_MB (1024UL * MATH_KB)
#define MATH_GB (1024UL * MATH_MB)
#define MATH_TB (1024UL * MATH_GB)

/**
 * @brief Converte byte in unità più leggibili
 */
__attribute__((unused)) static inline u64 math_bytes_to_kb(u64 bytes) {
  return bytes / MATH_KB;
}

__attribute__((unused)) static inline u64 math_bytes_to_mb(u64 bytes) {
  return bytes / MATH_MB;
}

__attribute__((unused)) static inline u64 math_kb_to_bytes(u64 kb) {
  return kb * MATH_KB;
}

__attribute__((unused)) static inline u64 math_mb_to_bytes(u64 mb) {
  return mb * MATH_MB;
}

/*
 * ============================================================================
 * CHECKSUM/CRC (per verifica integrità)
 * ============================================================================
 */

/**
 * @brief Checksum semplice
 */
u32 math_checksum(const void *data, size_t size);

/**
 * @brief CRC32 semplice
 */
u32 math_crc32(const void *data, size_t size);
