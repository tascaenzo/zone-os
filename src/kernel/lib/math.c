#include <lib/math.h>

/**
 * @file lib/math.c
 * @brief Implementazione minimalista delle funzioni matematiche per kernel
 *
 * Solo le funzioni essenziali per lo sviluppo kernel:
 * - Calcoli di memoria e allineamento
 * - Bit manipulation per hardware
 * - Utilità per algoritmi kernel
 */

/*
 * ============================================================================
 * GLOBAL STATE
 * ============================================================================
 */

// Stato per random number generator
static u32 math_rand_state = 1;

/*
 * ============================================================================
 * POWER AND ROOTS
 * ============================================================================
 */

u64 math_pow_int(u64 base, u32 exp) {
  if (exp == 0)
    return 1;
  if (base == 0)
    return 0;
  if (base == 1)
    return 1;

  u64 result = 1;
  u64 current_power = base;

  // Fast exponentiation by squaring
  while (exp > 0) {
    if (exp & 1) {
      // Check for overflow (semplificato)
      if (result > UINT64_MAX / current_power) {
        return UINT64_MAX; // Overflow
      }
      result *= current_power;
    }
    exp >>= 1;
    if (exp > 0) {
      if (current_power > UINT64_MAX / current_power) {
        return UINT64_MAX; // Overflow
      }
      current_power *= current_power;
    }
  }

  return result;
}

u32 math_sqrt_int(u64 x) {
  if (x == 0)
    return 0;
  if (x == 1)
    return 1;

  // Newton's method for integer square root
  u32 result = x;
  u32 prev;

  do {
    prev = result;
    result = (result + x / result) / 2;
  } while (result < prev);

  return prev;
}

/*
 * ============================================================================
 * RANDOM NUMBER GENERATION
 * ============================================================================
 */

void math_srand(u32 seed) {
  math_rand_state = seed ? seed : 1;
}

u32 math_rand(void) {
  // Linear Congruential Generator (LCG)
  // Using constants from Numerical Recipes
  math_rand_state = (math_rand_state * 1664525U + 1013904223U);
  return math_rand_state & MATH_RAND_MAX;
}

/*
 * ============================================================================
 * GREATEST COMMON DIVISOR
 * ============================================================================
 */

u64 math_gcd(u64 a, u64 b) {
  while (b != 0) {
    u64 temp = b;
    b = a % b;
    a = temp;
  }
  return a;
}

u64 math_lcm(u64 a, u64 b) {
  if (a == 0 || b == 0)
    return 0;

  u64 gcd = math_gcd(a, b);

  // Check overflow: lcm = (a * b) / gcd
  // Usiamo (a / gcd) * b per ridurre possibilità di overflow
  u64 a_div_gcd = a / gcd;
  if (a_div_gcd > UINT64_MAX / b) {
    return UINT64_MAX; // Overflow
  }

  return a_div_gcd * b;
}

/*
 * ============================================================================
 * CHECKSUM AND CRC
 * ============================================================================
 */

u32 math_checksum(const void *data, size_t size) {
  if (!data || size == 0)
    return 0;

  const u8 *bytes = (const u8 *)data;
  u32 checksum = 0;

  for (size_t i = 0; i < size; i++) {
    checksum += bytes[i];
  }

  return checksum;
}

u32 math_crc32(const void *data, size_t size) {
  if (!data || size == 0)
    return 0;

  // CRC32 table (simplified - usando solo alcuni valori)
  static const u32 crc_table[16] = {0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
                                    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};

  const u8 *bytes = (const u8 *)data;
  u32 crc = 0xFFFFFFFF;

  for (size_t i = 0; i < size; i++) {
    u8 byte = bytes[i];

    // Process byte in two 4-bit chunks
    crc = crc_table[(crc ^ byte) & 0x0F] ^ (crc >> 4);
    crc = crc_table[(crc ^ (byte >> 4)) & 0x0F] ^ (crc >> 4);
  }

  return crc ^ 0xFFFFFFFF;
}
