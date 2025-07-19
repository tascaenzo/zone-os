#pragma once

// Tipi interi senza segno
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// Tipi interi con segno
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;

// Limiti
#define UINT8_MAX ((uint8_t)0xFF)
#define UINT16_MAX ((uint16_t)0xFFFF)
#define UINT32_MAX ((uint32_t)0xFFFFFFFF)
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFULL)

#define INT8_MIN ((int8_t)-128)
#define INT8_MAX ((int8_t)127)
#define INT16_MIN ((int16_t)-32768)
#define INT16_MAX ((int16_t)32767)
#define INT32_MIN ((int32_t)-2147483648)
#define INT32_MAX ((int32_t)2147483647)
#define INT64_MIN ((int64_t)-9223372036854775807LL - 1)
#define INT64_MAX ((int64_t)9223372036854775807LL)
