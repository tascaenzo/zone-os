#include <drivers/video/console.h>
#include <klib/klog.h>
#include <lib/stdio.h>

// === Stato interno del sistema di logging ===

/// Livello minimo corrente per filtrare i messaggi di log
static klog_level_t current_log_level = KLOG_DEFAULT_LEVEL;

/// Flag per abilitare/disabilitare i colori nella console
static bool colors_enabled = true;

/// Stili predefiniti per ogni livello di log
static const klog_style_t log_styles[] = {
    [KLOG_LEVEL_DEBUG] = {CONSOLE_COLOR_DARK_GREY, CONSOLE_DEFAULT_BG, "[DEBUG]"}, // Grigio su nero
    [KLOG_LEVEL_INFO] = {CONSOLE_COLOR_GREEN, CONSOLE_DEFAULT_BG, "[INFO]"},       // Verde su nero
    [KLOG_LEVEL_WARN] = {CONSOLE_COLOR_YELLOW, CONSOLE_DEFAULT_BG, "[WARN]"},      // Giallo su nero
    [KLOG_LEVEL_ERROR] = {CONSOLE_COLOR_RED, CONSOLE_DEFAULT_BG, "[ERROR]"},       // Rosso su nero
    [KLOG_LEVEL_PANIC] = {CONSOLE_COLOR_WHITE, CONSOLE_COLOR_RED, "[PANIC]"},      // Bianco su rosso
};

// === API di configurazione ===

/**
 * @brief Imposta il livello minimo di log da stampare
 */
void klog_set_level(klog_level_t level) {
  if (level >= KLOG_LEVEL_DEBUG && level <= KLOG_LEVEL_PANIC) {
    current_log_level = level;
  }
}

/**
 * @brief Restituisce il livello di log corrente
 */
klog_level_t klog_get_level(void) {
  return current_log_level;
}

/**
 * @brief Abilita o disabilita l'uso dei colori nella console
 */
void klog_set_colors(bool enable) {
  colors_enabled = enable;
}

// === Utility interne ===

/**
 * @brief Imposta i colori della console secondo lo stile del livello specificato
 */
static void set_log_color(klog_level_t level) {
  if (!colors_enabled || level > KLOG_LEVEL_PANIC) {
    return;
  }
  const klog_style_t *style = &log_styles[level];
  console_set_color(style->fg_color, style->bg_color);
}

/**
 * @brief Ripristina i colori di default della console
 */
static void reset_log_color(void) {
  if (colors_enabled) {
    console_reset_colors();
  }
}

/**
 * @brief Stampa il prefisso testuale per un messaggio di log, con colori se abilitati
 */
static void print_log_prefix(klog_level_t level) {
  if (level > KLOG_LEVEL_PANIC) {
    kprintf("[UNKNOWN]");
    return;
  }

  set_log_color(level);
  kprintf("%s ", log_styles[level].prefix);
  reset_log_color();
}

// === API pubblica ===

/**
 * @brief Stampa un messaggio di log con livello specificato e formattazione stile printf
 */
void klog(klog_level_t level, const char *format, ...) {
  if (level < current_log_level)
    return;

  print_log_prefix(level);

  va_list args;
  va_start(args, format);
  kvprintf(format, args);
  va_end(args);

  kprintf("\n");
}

/**
 * @brief Log di livello DEBUG
 */
void klog_debug(const char *format, ...) {
  if (KLOG_LEVEL_DEBUG < current_log_level)
    return;

  print_log_prefix(KLOG_LEVEL_DEBUG);

  va_list args;
  va_start(args, format);
  kvprintf(format, args);
  va_end(args);

  kprintf("\n");
}

/**
 * @brief Log di livello INFO
 */
void klog_info(const char *format, ...) {
  if (KLOG_LEVEL_INFO < current_log_level)
    return;

  print_log_prefix(KLOG_LEVEL_INFO);

  va_list args;
  va_start(args, format);
  kvprintf(format, args);
  va_end(args);

  kprintf("\n");
}

/**
 * @brief Log di livello WARN
 */
void klog_warn(const char *format, ...) {
  if (KLOG_LEVEL_WARN < current_log_level)
    return;

  print_log_prefix(KLOG_LEVEL_WARN);

  va_list args;
  va_start(args, format);
  kvprintf(format, args);
  va_end(args);

  kprintf("\n");
}

/**
 * @brief Log di livello ERROR
 */
void klog_error(const char *format, ...) {
  if (KLOG_LEVEL_ERROR < current_log_level)
    return;

  print_log_prefix(KLOG_LEVEL_ERROR);

  va_list args;
  va_start(args, format);
  kvprintf(format, args);
  va_end(args);

  kprintf("\n");
}

/**
 * @brief Log di livello PANIC. Non può essere filtrato.
 *        Blocca il sistema dopo la stampa.
 */
void klog_panic(const char *format, ...) {
  print_log_prefix(KLOG_LEVEL_PANIC);

  va_list args;
  va_start(args, format);
  kvprintf(format, args);
  va_end(args);

  kprintf("\n");
  kprintf("\nKERNEL PANIC: System halted.\n");
  kprintf("This is a fatal error. The kernel cannot continue.\n");

  while (1) {
    __asm__ volatile("cli; hlt");
  }
}
