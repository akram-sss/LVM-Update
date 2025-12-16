#ifndef LVM_LOGGER_H
#define LVM_LOGGER_H

#include "lvm_types.h"
#include <stdarg.h>

// ─────────────────────────────────────────────────────
// ANSI COLOR CODES
// ─────────────────────────────────────────────────────
#define ANSI_RESET          "\033[0m"
#define ANSI_BOLD           "\033[1m"
#define ANSI_DIM            "\033[2m"

#define ANSI_BLACK          "\033[30m"
#define ANSI_RED            "\033[31m"
#define ANSI_GREEN          "\033[32m"
#define ANSI_YELLOW         "\033[33m"
#define ANSI_BLUE           "\033[34m"
#define ANSI_MAGENTA        "\033[35m"
#define ANSI_CYAN           "\033[36m"
#define ANSI_WHITE          "\033[37m"

#define ANSI_BG_RED         "\033[41m"
#define ANSI_BG_GREEN       "\033[42m"
#define ANSI_BG_YELLOW      "\033[43m"

// ─────────────────────────────────────────────────────
// LOGGING FUNCTIONS
// ─────────────────────────────────────────────────────

// Initialize logger
void log_init(void);

// Main logging function
void log_msg(log_level_t level, const char *component, const char *fmt, ...);

// Convenience macros for different log levels
#define LOG_DEBUG(component, ...) log_msg(LOG_DEBUG, component, __VA_ARGS__)
#define LOG_INFO(component, ...) log_msg(LOG_INFO, component, __VA_ARGS__)
#define LOG_SUCCESS(component, ...) log_msg(LOG_SUCCESS, component, __VA_ARGS__)
#define LOG_WARN(component, ...) log_msg(LOG_WARNING, component, __VA_ARGS__)
#define LOG_ERROR(component, ...) log_msg(LOG_ERROR, component, __VA_ARGS__)
#define LOG_CRITICAL(component, ...) log_msg(LOG_CRITICAL, component, __VA_ARGS__)

// Special formatted output functions
void print_banner(void);
void print_separator(void);
void print_config_summary(void);
void print_volume_status(vol_status_t *vol);
void print_statistics(void);
void print_operation_result(int success, const char *operation, const char *details);

// Progress indicator
void print_progress(const char *operation, int current, int total);

#endif // LVM_LOGGER_H
