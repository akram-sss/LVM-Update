#ifndef LVM_TYPES_H
#define LVM_TYPES_H

#include <time.h>
#include <pthread.h>
#include "lvm_config.h"

// ─────────────────────────────────────────────────────
// ENUMERATIONS
// ─────────────────────────────────────────────────────

// Volume state classification
typedef enum {
    LV_OK = 0,              // Normal operation
    LV_HUNGRY,              // Needs more space (usage >= threshold)
    LV_OVERPROVISIONED      // Too much free space (usage < low threshold)
} lv_state_t;

// Log severity levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_SUCCESS,
    LOG_WARNING,
    LOG_ERROR,
    LOG_CRITICAL
} log_level_t;

// ─────────────────────────────────────────────────────
// STRUCTURES
// ─────────────────────────────────────────────────────

// Volume status tracking
typedef struct {
    char device[128];           // Device path (e.g., /dev/mapper/vgdata-lv_home)
    char mountpoint[256];       // Mount point (e.g., /mnt/lv_home)
    char vg_name[128];          // Volume group name
    char lv_name[128];          // Logical volume name
    char fs_type[32];           // Filesystem type (ext4, xfs, etc.)
    
    int use_pct;                // Current usage percentage
    long long size_bytes;       // Total size in bytes
    long long used_bytes;       // Used space in bytes
    long long free_bytes;       // Free space in bytes
    
    time_t last_action;         // Timestamp of last operation
    time_t first_seen;          // When volume was first detected
    char last_msg[512];         // Last status message
    
    int history[HISTORY_SAMPLES];   // Rolling window of usage percentages
    int history_pos;                // Current position in ring buffer
    int history_filled;             // Number of valid samples in history
    
    int extension_count;        // Number of times extended
    int shrink_count;           // Number of times shrunk
} vol_status_t;

// Global statistics
typedef struct {
    unsigned long checks_performed;
    unsigned long extensions_succeeded;
    unsigned long extensions_failed;
    unsigned long shrinks_performed;
    unsigned long fallback_pvs_added;
    time_t start_time;
    time_t last_check;
} system_stats_t;

// Pending operation queue entry
typedef struct {
    char device[256];
    lv_state_t state;
    int priority;
    time_t queued_at;
} pending_op_t;

// ─────────────────────────────────────────────────────
// GLOBAL STATE
// ─────────────────────────────────────────────────────

// Volume tracking
extern vol_status_t volumes[MAX_VOLUMES];
extern int volumes_count;
extern pthread_mutex_t volumes_mutex;

// Pending operations queue
extern char pending_device[256];
extern pthread_mutex_t pending_mutex;
extern pthread_cond_t pending_cond;

// System statistics
extern system_stats_t sys_stats;
extern pthread_mutex_t stats_mutex;

// Shutdown flag
extern volatile int shutdown_requested;

#endif // LVM_TYPES_H
