#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include "lvm_logger.h"
#include "lvm_config.h"

// ─────────────────────────────────────────────────────
// INTERNAL STATE
// ─────────────────────────────────────────────────────
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static int use_colors = LOG_USE_COLORS;

// ─────────────────────────────────────────────────────
// INITIALIZATION
// ─────────────────────────────────────────────────────
void log_init(void) {
    // Check if output is a terminal
    if (!isatty(STDOUT_FILENO)) {
        use_colors = 0;
    }
}

// ─────────────────────────────────────────────────────
// HELPER: Get color for log level
// ─────────────────────────────────────────────────────
static const char* get_level_color(log_level_t level) {
    if (!use_colors) return "";
    
    switch (level) {
        case LOG_DEBUG:     return ANSI_DIM ANSI_CYAN;
        case LOG_INFO:      return ANSI_BLUE;
        case LOG_SUCCESS:   return ANSI_GREEN;
        case LOG_WARNING:   return ANSI_YELLOW;
        case LOG_ERROR:     return ANSI_RED;
        case LOG_CRITICAL:  return ANSI_BOLD ANSI_BG_RED ANSI_WHITE;
        default:            return ANSI_RESET;
    }
}

// ─────────────────────────────────────────────────────
// HELPER: Get label for log level
// ─────────────────────────────────────────────────────
static const char* get_level_label(log_level_t level) {
    switch (level) {
        case LOG_DEBUG:     return "DEBUG";
        case LOG_INFO:      return "INFO ";
        case LOG_SUCCESS:   return "OK   ";
        case LOG_WARNING:   return "WARN ";
        case LOG_ERROR:     return "ERROR";
        case LOG_CRITICAL:  return "CRIT ";
        default:            return "     ";
    }
}

// ─────────────────────────────────────────────────────
// MAIN LOGGING FUNCTION
// ─────────────────────────────────────────────────────
void log_msg(log_level_t level, const char *component, const char *fmt, ...) {
    pthread_mutex_lock(&log_mutex);
    
    char timestamp[32] = "";
    char thread_info[32] = "";
    
    // Get timestamp
    if (LOG_SHOW_TIMESTAMPS) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    }
    
    // Get thread ID
    if (LOG_SHOW_THREAD_ID) {
        pid_t tid = syscall(SYS_gettid);
        snprintf(thread_info, sizeof(thread_info), "[%d]", tid);
    }
    
    // Print log header
    const char *color = get_level_color(level);
    const char *reset = use_colors ? ANSI_RESET : "";
    const char *label = get_level_label(level);
    const char *comp_color = use_colors ? ANSI_BOLD : "";
    
    fprintf(stdout, "%s[%s]%s ", color, label, reset);
    
    if (LOG_SHOW_TIMESTAMPS) {
        fprintf(stdout, "%s%s%s ", use_colors ? ANSI_DIM : "", timestamp, reset);
    }
    
    if (LOG_SHOW_THREAD_ID && thread_info[0]) {
        fprintf(stdout, "%s%s%s ", use_colors ? ANSI_DIM : "", thread_info, reset);
    }
    
    if (component && component[0]) {
        fprintf(stdout, "%s%-12s%s│ ", comp_color, component, reset);
    }
    
    // Print message
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    
    fprintf(stdout, "\n");
    fflush(stdout);
    
    pthread_mutex_unlock(&log_mutex);
}

// ─────────────────────────────────────────────────────
// BANNER
// ─────────────────────────────────────────────────────
void print_banner(void) {
    const char *cyan = use_colors ? ANSI_CYAN : "";
    const char *bold = use_colors ? ANSI_BOLD : "";
    const char *reset = use_colors ? ANSI_RESET : "";
    const char *green = use_colors ? ANSI_GREEN : "";
    const char *dim = use_colors ? ANSI_DIM : "";
    
    printf("\n");
    printf("%s%s╔══════════════════════════════════════════════════════════════╗%s\n", bold, cyan, reset);
    printf("%s%s║           LVM AUTO-EXTENDER FOR RED HAT LINUX               ║%s\n", bold, cyan, reset);
    printf("%s%s╠══════════════════════════════════════════════════════════════╣%s\n", bold, cyan, reset);
    printf("%s%s║  %sAutomatic Logical Volume Management & Optimization      %s%s║%s\n", 
           bold, cyan, reset, cyan, bold, reset);
    printf("%s%s║  %sIntelligent space redistribution & load balancing        %s%s║%s\n", 
           bold, cyan, reset, cyan, bold, reset);
    printf("%s%s╚══════════════════════════════════════════════════════════════╝%s\n", bold, cyan, reset);
    printf("\n");
}

// ─────────────────────────────────────────────────────
// SEPARATOR
// ─────────────────────────────────────────────────────
void print_separator(void) {
    const char *dim = use_colors ? ANSI_DIM : "";
    const char *reset = use_colors ? ANSI_RESET : "";
    printf("%s────────────────────────────────────────────────────────────────%s\n", dim, reset);
}

// ─────────────────────────────────────────────────────
// CONFIG SUMMARY
// ─────────────────────────────────────────────────────
void print_config_summary(void) {
    const char *yellow = use_colors ? ANSI_YELLOW : "";
    const char *green = use_colors ? ANSI_GREEN : "";
    const char *bold = use_colors ? ANSI_BOLD : "";
    const char *reset = use_colors ? ANSI_RESET : "";
    const char *dim = use_colors ? ANSI_DIM : "";
    
    printf("%s%s┌─ CONFIGURATION ────────────────────────────────────────────┐%s\n", bold, dim, reset);
    
    if (DRY_RUN) {
        printf("%s%s│ Mode:              %s⚠  DRY-RUN (Simulation)               %s%s│%s\n", 
               dim, bold, yellow, dim, bold, reset);
    } else {
        printf("%s%s│ Mode:              %s✓  PRODUCTION (Real operations)       %s%s│%s\n", 
               dim, bold, green, dim, bold, reset);
    }
    
    printf("%s%s│ Check Interval:    %s%d seconds                            %s%s│%s\n", 
           dim, bold, reset, CHECK_INTERVAL, dim, bold, reset);
    printf("%s%s│ Hungry Threshold:  %s>= %d%%                               %s%s│%s\n", 
           dim, bold, reset, THRESHOLD_PCT, dim, bold, reset);
    printf("%s%s│ Low Threshold:     %s< %d%%                                %s%s│%s\n", 
           dim, bold, reset, LOW_PCT, dim, bold, reset);
    printf("%s%s│ Extension Size:    %s%d GB per operation                   %s%s│%s\n", 
           dim, bold, reset, EXTEND_SIZE_GB, dim, bold, reset);
    printf("%s%s│ Fallback Device:   %s%-35s%s%s│%s\n", 
           dim, bold, reset, FALLBACK_DEV, dim, bold, reset);
    printf("%s%s│ Dashboard Port:    %s%d                                    %s%s│%s\n", 
           dim, bold, reset, DASHBOARD_PORT, dim, bold, reset);
    printf("%s%s└────────────────────────────────────────────────────────────┘%s\n", bold, dim, reset);
    printf("\n");
}

// ─────────────────────────────────────────────────────
// VOLUME STATUS
// ─────────────────────────────────────────────────────
void print_volume_status(vol_status_t *vol) {
    const char *bold = use_colors ? ANSI_BOLD : "";
    const char *reset = use_colors ? ANSI_RESET : "";
    const char *green = use_colors ? ANSI_GREEN : "";
    const char *yellow = use_colors ? ANSI_YELLOW : "";
    const char *red = use_colors ? ANSI_RED : "";
    
    const char *usage_color = reset;
    if (vol->use_pct >= THRESHOLD_PCT) usage_color = red;
    else if (vol->use_pct >= 70) usage_color = yellow;
    else usage_color = green;
    
    printf("\n%s┌─ Volume Status ──────────────────────────────────────────────┐%s\n", bold, reset);
    printf("%s│%s %-62s%s│%s\n", bold, reset, vol->mountpoint, bold, reset);
    printf("%s├──────────────────────────────────────────────────────────────┤%s\n", bold, reset);
    printf("%s│%s Device:     %-51s%s│%s\n", bold, reset, vol->device, bold, reset);
    printf("%s│%s VG/LV:      %-51s%s│%s\n", bold, reset, 
           (vol->vg_name[0] ? vol->vg_name : "N/A"), bold, reset);
    printf("%s│%s Filesystem: %-51s%s│%s\n", bold, reset, 
           (vol->fs_type[0] ? vol->fs_type : "unknown"), bold, reset);
    printf("%s│%s Usage:      %s%3d%%%s %-46s%s│%s\n", 
           bold, reset, usage_color, vol->use_pct, reset, "", bold, reset);
    printf("%s│%s Extensions: %-51d%s│%s\n", bold, reset, vol->extension_count, bold, reset);
    printf("%s│%s Status:     %-51s%s│%s\n", bold, reset, 
           (vol->last_msg[0] ? vol->last_msg : "OK"), bold, reset);
    printf("%s└──────────────────────────────────────────────────────────────┘%s\n", bold, reset);
}

// ─────────────────────────────────────────────────────
// STATISTICS
// ─────────────────────────────────────────────────────
void print_statistics(void) {
    extern system_stats_t sys_stats;
    
    const char *bold = use_colors ? ANSI_BOLD : "";
    const char *reset = use_colors ? ANSI_RESET : "";
    const char *cyan = use_colors ? ANSI_CYAN : "";
    const char *green = use_colors ? ANSI_GREEN : "";
    
    time_t uptime = time(NULL) - sys_stats.start_time;
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    printf("\n%s%s╔═ SYSTEM STATISTICS ═══════════════════════════════════════╗%s\n", bold, cyan, reset);
    printf("%s%s║%s Uptime:              %02d:%02d:%02d                             %s%s║%s\n", 
           bold, cyan, reset, hours, minutes, seconds, cyan, bold, reset);
    printf("%s%s║%s Checks Performed:    %-37lu%s%s║%s\n", 
           bold, cyan, reset, sys_stats.checks_performed, cyan, bold, reset);
    printf("%s%s║%s Extensions Success:  %s%-37lu%s%s║%s\n", 
           bold, cyan, reset, green, sys_stats.extensions_succeeded, cyan, bold, reset);
    printf("%s%s║%s Extensions Failed:   %-37lu%s%s║%s\n", 
           bold, cyan, reset, sys_stats.extensions_failed, cyan, bold, reset);
    printf("%s%s║%s Shrinks Performed:   %-37lu%s%s║%s\n", 
           bold, cyan, reset, sys_stats.shrinks_performed, cyan, bold, reset);
    printf("%s%s║%s Fallback PVs Added:  %-37lu%s%s║%s\n", 
           bold, cyan, reset, sys_stats.fallback_pvs_added, cyan, bold, reset);
    printf("%s%s╚═══════════════════════════════════════════════════════════╝%s\n", bold, cyan, reset);
    printf("\n");
}

// ─────────────────────────────────────────────────────
// OPERATION RESULT
// ─────────────────────────────────────────────────────
void print_operation_result(int success, const char *operation, const char *details) {
    const char *status_color = use_colors ? (success ? ANSI_GREEN : ANSI_RED) : "";
    const char *reset = use_colors ? ANSI_RESET : "";
    const char *bold = use_colors ? ANSI_BOLD : "";
    const char *symbol = success ? "✓" : "✗";
    
    printf("\n%s%s%s %s: %s%s%s\n", bold, status_color, symbol, operation, reset, details, reset);
}

// ─────────────────────────────────────────────────────
// PROGRESS INDICATOR
// ─────────────────────────────────────────────────────
void print_progress(const char *operation, int current, int total) {
    const char *cyan = use_colors ? ANSI_CYAN : "";
    const char *reset = use_colors ? ANSI_RESET : "";
    
    int percent = (current * 100) / total;
    int bars = (percent * 40) / 100;
    
    printf("\r%s%s:%s [", cyan, operation, reset);
    for (int i = 0; i < 40; i++) {
        printf("%s", i < bars ? "█" : "░");
    }
    printf("] %3d%%", percent);
    fflush(stdout);
    
    if (current >= total) {
        printf("\n");
    }
}
