#pragma once

#include <lib/stdarg.h>
#include <lib/types.h>

/// Struttura che rappresenta lo stile di un messaggio di log
typedef struct {
  uint32_t fg_color;  ///< Colore del testo (ARGB)
  uint32_t bg_color;  ///< Colore dello sfondo (ARGB)
  const char *prefix; ///< Prefisso testuale (es: "[ERROR]")
} klog_style_t;

// === Log Levels ===
typedef enum {
  KLOG_LEVEL_DEBUG = 0, // Dettagli interni, solo in debug builds
  KLOG_LEVEL_INFO = 1,  // Informazioni generali
  KLOG_LEVEL_WARN = 2,  // Warning non critici
  KLOG_LEVEL_ERROR = 3, // Errori gravi ma recuperabili
  KLOG_LEVEL_PANIC = 4, // Errori fatali - kernel panic
} klog_level_t;

// === Configurazione ===

/**
 * @brief Imposta il livello minimo di log da mostrare
 * @param level Solo i log >= questo livello verranno stampati
 */
void klog_set_level(klog_level_t level);

/**
 * @brief Ottiene il livello corrente di log
 * @return Livello minimo attualmente configurato
 */
klog_level_t klog_get_level(void);

/**
 * @brief Abilita/disabilita i colori nei log
 * @param enable true per abilitare colori, false per disabilitare
 */
void klog_set_colors(bool enable);

// === API di Logging ===

/**
 * @brief Log generico con livello specificato
 * @param level Livello del messaggio
 * @param format String di formato (come kprintf)
 * @param ... Argomenti per il formato
 */
void klog(klog_level_t level, const char *format, ...);

/**
 * @brief Log di debug - dettagli interni
 * @param format String di formato
 * @param ... Argomenti
 */
void klog_debug(const char *format, ...);

/**
 * @brief Log informativo - operazioni normali
 * @param format String di formato
 * @param ... Argomenti
 */
void klog_info(const char *format, ...);

/**
 * @brief Log di warning - situazioni anomale ma non critiche
 * @param format String di formato
 * @param ... Argomenti
 */
void klog_warn(const char *format, ...);

/**
 * @brief Log di errore - problemi gravi
 * @param format String di formato
 * @param ... Argomenti
 */
void klog_error(const char *format, ...);

/**
 * @brief Log di panic - errore fatale, termina il kernel
 * @param format String di formato
 * @param ... Argomenti
 * @note Questa funzione non ritorna mai
 */
void klog_panic(const char *format, ...) __attribute__((noreturn));

// === Utility Macros ===

// Macro per debug condizionale (solo in debug builds)
#ifdef DEBUG
#define KLOG_DEBUG(fmt, ...) klog_debug(fmt, ##__VA_ARGS__)
#else
#define KLOG_DEBUG(fmt, ...) ((void)0)
#endif

// Macro per assert con log
#define KLOG_ASSERT(condition, fmt, ...)                                                                                                                                           \
  do {                                                                                                                                                                             \
    if (!(condition)) {                                                                                                                                                            \
      klog_panic("ASSERTION FAILED: %s at %s:%d - " fmt, #condition, __FILE__, __LINE__, ##__VA_ARGS__);                                                                           \
    }                                                                                                                                                                              \
  } while (0)

// === Constants ===
#define KLOG_MAX_MESSAGE_LEN 512 // Lunghezza massima messaggio

// Livelli di default
#ifdef DEBUG
#define KLOG_DEFAULT_LEVEL KLOG_LEVEL_DEBUG
#else
#define KLOG_DEFAULT_LEVEL KLOG_LEVEL_INFO
#endif
