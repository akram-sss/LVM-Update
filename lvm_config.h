#ifndef LVM_CONFIG_H
#define LVM_CONFIG_H

//! =====================================================
//  LVM AUTO-EXTENDER - CONFIGURATION
//  Optimized for Red Hat Enterprise Linux / CentOS
//! =====================================================

// ─────────────────────────────────────────────────────
// OPERATION MODE
// ─────────────────────────────────────────────────────
#define DRY_RUN                 1       // 1 = simulation mode, 0 = execute real LVM operations

// ─────────────────────────────────────────────────────
// MONITORING THRESHOLDS
// ─────────────────────────────────────────────────────
#define CHECK_INTERVAL          8       // seconds between filesystem checks
#define THRESHOLD_PCT           80      // usage % to trigger auto-extension (HUNGRY state)
#define LOW_PCT                 40      // usage % threshold for over-provisioned detection
#define HISTORY_SAMPLES         12      // rolling window samples (~96 seconds at 8s intervals)

// ─────────────────────────────────────────────────────
// EXTENSION PARAMETERS
// ─────────────────────────────────────────────────────
#define EXTEND_SIZE_GB          1       // size in GB to extend/shrink per operation
#define MIN_FREE_FOR_DONOR_GB   1       // minimum free space in GB required to shrink from donor

// ─────────────────────────────────────────────────────
// STORAGE CONFIGURATION
// ─────────────────────────────────────────────────────
#define FALLBACK_DEV            "/dev/sdc"                  // fallback physical volume
#define LOCK_FILE               "/var/lock/lvm_extender.lock"  // RHEL-compliant lock location
#define MONITORED_MOUNTS        "/mnt/lv_home","/mnt/lv_data1","/mnt/lv_data2"

// ─────────────────────────────────────────────────────
// SUPPORTED FILESYSTEMS (Red Hat optimized)
// ─────────────────────────────────────────────────────
#define SUPPORTED_FS_TYPES      "ext4,xfs"  // XFS is default on RHEL 7+

// ─────────────────────────────────────────────────────
// HTTP DASHBOARD
// ─────────────────────────────────────────────────────
#define DASHBOARD_PORT          8080
#define DASHBOARD_ENABLED       1

// ─────────────────────────────────────────────────────
// LOAD GENERATOR (for testing)
// ─────────────────────────────────────────────────────
#define WRITER_ENABLED          1
#define WRITER_BASE_PATH        "/mnt/lv_home"
#define WRITER_FILE_SIZE_MB     1
#define WRITER_SLEEP_USEC       20000   // microseconds between writes
#define WRITER_COUNT            2       // number of writer threads

// ─────────────────────────────────────────────────────
// SYSTEM LIMITS
// ─────────────────────────────────────────────────────
#define MAX_VOLUMES             64
#define MAX_COMMAND_LEN         1024
#define MAX_BUFFER_LEN          8192

// ─────────────────────────────────────────────────────
// LOGGING
// ─────────────────────────────────────────────────────
#define LOG_USE_COLORS          1       // enable ANSI colors in terminal
#define LOG_SHOW_TIMESTAMPS     1       // show timestamps in logs
#define LOG_SHOW_THREAD_ID      1       // show thread info in logs

#endif // LVM_CONFIG_H
