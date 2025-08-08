#include "stdio.h"
#include <drivers/video/console.h>
#include <lib/stdarg.h>
#include <lib/string/string.h>
#include <lib/types.h>

static int print_number(char *buf, uint64_t num, int base, int is_signed, int uppercase) {
  static const char *digits_l = "0123456789abcdef";
  static const char *digits_u = "0123456789ABCDEF";
  const char *digits = uppercase ? digits_u : digits_l;
  char temp[64];
  int i = 0;
  int chars_written = 0;

  if (is_signed && ((int64_t)num) < 0) {
    *buf++ = '-';
    chars_written++;
    num = -(int64_t)num;
  }

  // Handle special case of 0
  if (num == 0) {
    *buf++ = '0';
    return chars_written + 1;
  }

  do {
    temp[i++] = digits[num % base];
    num /= base;
  } while (num);

  for (int j = i - 1; j >= 0; j--) {
    *buf++ = temp[j];
  }

  return chars_written + i;
}

static int kvsprintf(char *buf, const char *fmt, va_list args) {
  char *start = buf;
  while (*fmt) {
    if (*fmt != '%') {
      *buf++ = *fmt++;
      continue;
    }
    fmt++;

    // Gestione flag (per ora ignoriamo, ma dobbiamo saltarli)
    while (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#' || *fmt == '0') {
      fmt++;
    }

    // Gestione larghezza (per ora ignoriamo, ma dobbiamo saltarla)
    while (*fmt >= '0' && *fmt <= '9') {
      fmt++;
    }

    // Gestione precisione (per ora ignoriamo, ma dobbiamo saltarla)
    if (*fmt == '.') {
      fmt++;
      while (*fmt >= '0' && *fmt <= '9') {
        fmt++;
      }
    }

    // Gestione dei modificatori 'l'
    int long_modifier = 0;
    while (*fmt == 'l') {
      long_modifier++;
      fmt++;
    }

    int size_t_modifier = 0;
    if (*fmt == 'z') {
      size_t_modifier = 1;
      fmt++;
    }

    switch (*fmt) {
    case 'd':
    case 'i':
      if (long_modifier >= 2) {
        // %lld o %lli
        long long val = va_arg(args, long long);
        buf += print_number(buf, (uint64_t)val, 10, 1, 0);
      } else if (long_modifier == 1) {
        // %ld o %li
        long val = va_arg(args, long);
        buf += print_number(buf, (uint64_t)val, 10, 1, 0);
      } else {
        // %d o %i
        int val = va_arg(args, int);
        buf += print_number(buf, (uint64_t)val, 10, 1, 0);
      }
      break;
    case 'u':
      if (size_t_modifier) {
        size_t val = va_arg(args, size_t);
        buf += print_number(buf, (uint64_t)val, 10, 0, 0);
      } else if (long_modifier >= 2) {
        unsigned long long val = va_arg(args, unsigned long long);
        buf += print_number(buf, (uint64_t)val, 10, 0, 0);
      } else if (long_modifier == 1) {
        unsigned long val = va_arg(args, unsigned long);
        buf += print_number(buf, (uint64_t)val, 10, 0, 0);
      } else {
        unsigned int val = va_arg(args, unsigned int);
        buf += print_number(buf, (uint64_t)val, 10, 0, 0);
      }
      break;
    case 'x':
      if (long_modifier >= 2) {
        unsigned long long val = va_arg(args, unsigned long long);
        buf += print_number(buf, (uint64_t)val, 16, 0, 0);
      } else if (long_modifier == 1) {
        unsigned long val = va_arg(args, unsigned long);
        buf += print_number(buf, (uint64_t)val, 16, 0, 0);
      } else {
        unsigned int val = va_arg(args, unsigned int);
        buf += print_number(buf, (uint64_t)val, 16, 0, 0);
      }
      break;
    case 'X':
      if (long_modifier >= 2) {
        unsigned long long val = va_arg(args, unsigned long long);
        buf += print_number(buf, (uint64_t)val, 16, 0, 1);
      } else if (long_modifier == 1) {
        unsigned long val = va_arg(args, unsigned long);
        buf += print_number(buf, (uint64_t)val, 16, 0, 1);
      } else {
        unsigned int val = va_arg(args, unsigned int);
        buf += print_number(buf, (uint64_t)val, 16, 0, 1);
      }
      break;
    case 'o':
      if (long_modifier >= 2) {
        unsigned long long val = va_arg(args, unsigned long long);
        buf += print_number(buf, (uint64_t)val, 8, 0, 0);
      } else if (long_modifier == 1) {
        unsigned long val = va_arg(args, unsigned long);
        buf += print_number(buf, (uint64_t)val, 8, 0, 0);
      } else {
        unsigned int val = va_arg(args, unsigned int);
        buf += print_number(buf, (uint64_t)val, 8, 0, 0);
      }
      break;
    case 'c':
      *buf++ = (char)va_arg(args, int);
      break;
    case 's': {
      const char *s = va_arg(args, const char *);
      if (s) {
        while (*s)
          *buf++ = *s++;
      } else {
        // Handle NULL string
        const char *null_str = "(null)";
        while (*null_str)
          *buf++ = *null_str++;
      }
      break;
    }
    case 'p':
      *buf++ = '0';
      *buf++ = 'x';
      buf += print_number(buf, (uint64_t)(uptr)va_arg(args, void *), 16, 0, 0);
      break;
    case '%':
      *buf++ = '%';
      break;
    default:
      *buf++ = '?';
      break;
    }
    fmt++;
  }
  *buf = '\0';
  return buf - start;
}

int kvprintf(const char *fmt, va_list args) {
  char buffer[1024];
  int len = kvsprintf(buffer, fmt, args);
  console_write(buffer);
  return len;
}

int kprintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int ret = kvprintf(fmt, args);
  va_end(args);
  return ret;
}

int ksprintf(char *buf, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int len = kvsprintf(buf, fmt, args);
  va_end(args);
  return len;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char temp[1024];
  int len = kvsprintf(temp, fmt, args);
  va_end(args);
  if (size > 0) {
    strncpy(buf, temp, size - 1);
    buf[size - 1] = '\0';
  }
  return len;
}

int kputchar(int c) {
  char ch = (char)c;
  console_putc(ch);
  return c;
}

int kputs(const char *s) {
  kprintf("%s\n", s);
  return 0;
}