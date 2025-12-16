#ifndef LVM_UTILS_H
#define LVM_UTILS_H

#include "lvm_types.h"

// ─────────────────────────────────────────────────────
// VOLUME MANAGEMENT
// ─────────────────────────────────────────────────────

// Get or create volume tracking entry
vol_status_t* get_or_create_volume(const char *device, const char *mountpoint);

// Update volume status
void update_volume_status(const char *device, const char *mountpoint,
                         int use_pct, const char *msg);

// Classify volume state (OK, HUNGRY, OVERPROVISIONED)
lv_state_t classify_lv(vol_status_t *v);

// Find volume by device name
vol_status_t* find_volume_by_device(const char *device);

// ─────────────────────────────────────────────────────
// FILESYSTEM SCANNING
// ─────────────────────────────────────────────────────

// Parse df output and return filesystem info
int parse_df_and_find_full(char out_devices[][256], char out_mounts[][256],
                           int out_use[], int *out_count);

// Check if mount point should be monitored
int should_monitor_mount(const char *mountpoint);

// ─────────────────────────────────────────────────────
// LVM OPERATIONS
// ─────────────────────────────────────────────────────

// Get VG and LV names from device path
int get_vg_lv(const char *device, char *vg, size_t vgsz, char *lv, size_t lvsz);

// Get VG free space in bytes
long long get_vg_free_space(const char *vg_name);

// Get filesystem type for a device
int get_filesystem_type(const char *vg, const char *lv, char *fs_type, size_t fs_size);

// Get free space inside filesystem in bytes
long long get_fs_free_space(const char *vg, const char *lv);

// Check if filesystem can be safely shrunk
int can_shrink_filesystem(const char *fs_type);

// Check if device is already a PV
int is_physical_volume(const char *device);

// ─────────────────────────────────────────────────────
// STATISTICS
// ─────────────────────────────────────────────────────

// Update system statistics
void stats_increment_checks(void);
void stats_increment_extension_success(void);
void stats_increment_extension_fail(void);
void stats_increment_shrink(void);
void stats_increment_fallback_pv(void);

// ─────────────────────────────────────────────────────
// UTILITIES
// ─────────────────────────────────────────────────────

// Execute command and return output
int execute_command(const char *cmd, char *output, size_t output_size);

// Format bytes to human-readable string
void format_bytes(long long bytes, char *output, size_t size);

// Check if file/device exists
int file_exists(const char *path);

#endif // LVM_UTILS_H
