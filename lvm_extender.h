#ifndef LVM_EXTENDER_H
#define LVM_EXTENDER_H

// ─────────────────────────────────────────────────────
// LVM EXTENSION OPERATIONS
// ─────────────────────────────────────────────────────

// Main extender function - tries to extend a hungry LV
// Returns: 0 on success, -1 on failure
int try_extender_for_device(const char *device);

// Shrink donor LVs to free space in VG
// Returns: bytes freed
long long shrink_donor_lvs(const char *vg_name, const char *target_lv, long long needed_bytes);

// Extend LV if VG has enough free space
// Returns: 0 on success, -1 on failure
int extend_lv(const char *vg_name, const char *lv_name, long long size_bytes);

// Add fallback PV to VG if needed
// Returns: 0 on success, -1 on failure
int add_fallback_pv(const char *vg_name, const char *fallback_device);

// Execute LVM command (respects DRY_RUN mode)
// Returns: 0 on success, -1 on failure
int execute_lvm_command(const char *cmd, const char *description);

#endif // LVM_EXTENDER_H
