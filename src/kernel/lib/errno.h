/**
 * @file    errno.h
 * @brief   Codici di errore minimi per kernel
 *
 * Convenzione: valori negativi per indicare errore (-EINVAL, -ENODEV, ...).
 *
 * @author Enzo Tasca
 * @date 2025
 */

#pragma once
#include <lib/stdbool.h>
#include <lib/stdint.h>

/* Tipo alias per error code */
typedef int errno_t;

/* Errori base sufficienti per bootstrap/memoria */
#define EINVAL 22 /* Invalid argument */
#define ENODEV 19 /* No such device */
#define ENOENT 2  /* No such entry / file */
#define ENOMEM 12 /* Out of memory */

/* Helper macro */
static inline bool ERR_IS_NEG(long v) {
  return v < 0;
}
static inline int ERRNO_ABS(long v) {
  return (v < 0) ? (int)(-v) : (int)v;
}
