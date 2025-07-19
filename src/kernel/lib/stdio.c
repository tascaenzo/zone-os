#include <drivers/video/console.h>
#include <lib/stdarg.h>
#include <lib/string.h>
#include <lib/types.h>

static int print_number(char *buf, uint64_t num, int base, int is_signed, int uppercase) {
  static const char *digits_l = "0123456789abcdef";
  static const char *digits_u = "0123456789ABCDEF";
  const char *digits = uppercase ? digits_u : digits_l;
  char temp[32];
  int i = 0;

  if (is_signed && ((int64_t)num) < 0) {
    *buf++ = '-';
    num = -(int64_t)num;
  }

  do {
    temp[i++] = digits[num % base];
    num /= base;
  } while (num);

  for (int j = i - 1; j >= 0; j--) {
    *buf++ = temp[j];
  }

  return i;
}

static int kvsprintf(char *buf, const char *fmt, va_list args) {
  char *start = buf;
  while (*fmt) {
    if (*fmt != '%') {
      *buf++ = *fmt++;
      continue;
    }
    fmt++;

    switch (*fmt) {
    case 'd':
    case 'i':
      buf += print_number(buf, va_arg(args, int), 10, 1, 0);
      break;
    case 'u':
      buf += print_number(buf, va_arg(args, unsigned int), 10, 0, 0);
      break;
    case 'x':
      buf += print_number(buf, va_arg(args, unsigned int), 16, 0, 0);
      break;
    case 'X':
      buf += print_number(buf, va_arg(args, unsigned int), 16, 0, 1);
      break;
    case 'o':
      buf += print_number(buf, va_arg(args, unsigned int), 8, 0, 0);
      break;
    case 'c':
      *buf++ = (char)va_arg(args, int);
      break;
    case 's': {
      const char *s = va_arg(args, const char *);
      while (*s)
        *buf++ = *s++;
      break;
    }
    case 'p':
      *buf++ = '0';
      *buf++ = 'x';
      buf += print_number(buf, (uptr)va_arg(args, void *), 16, 0, 0);
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
