#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lvm_extender.h"
#include "lvm_logger.h"
#include "lvm_utils.h"
#include "lvm_config.h"

// ─────────────────────────────────────────────────────
// EXECUTE LVM COMMAND
// ─────────────────────────────────────────────────────
int execute_lvm_command(const char *cmd, const char *description) {
    if (DRY_RUN) {
        LOG_WARN("Extender", "[DRY-RUN] Would execute: %s", description);
        LOG_DEBUG("Extender", "Command: %s", cmd);
        return 0;
    }
    
    LOG_INFO("Extender", "Executing: %s", description);
    LOG_DEBUG("Extender", "Command: %s", cmd);
    
    int ret = system(cmd);
    
    if (ret == 0) {
        LOG_SUCCESS("Extender", "Successfully executed: %s", description);
        return 0;
    } else {
        LOG_ERROR("Extender", "Failed to execute: %s (exit code: %d)", description, ret);
        return -1;
    }
}

// ─────────────────────────────────────────────────────
// SHRINK DONOR LVs
// ─────────────────────────────────────────────────────
long long shrink_donor_lvs(const char *vg_name, const char *target_lv, long long needed_bytes) {
    char cmd[MAX_COMMAND_LEN];
    char buf[1024];
    long long bytes_freed = 0;
    long long min_free_bytes = (long long)MIN_FREE_FOR_DONOR_GB * 1024 * 1024 * 1024;
    long long shrink_size = (long long)EXTEND_SIZE_GB * 1024 * 1024 * 1024;
    
    LOG_INFO("Extender", "Searching for donor LVs in VG '%s' (need %lld bytes)", 
             vg_name, needed_bytes);
    
    // Get list of LVs in this VG
    snprintf(cmd, sizeof(cmd),
             "lvs --noheadings -o lv_name,lv_size --units b --nosuffix %s 2>/dev/null",
             vg_name);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        LOG_ERROR("Extender", "Failed to list LVs in VG '%s'", vg_name);
        return 0;
    }
    
    int donors_found = 0;
    
    while (fgets(buf, sizeof(buf), fp)) {
        char lv_name[128];
        long long lv_size;
        
        if (sscanf(buf, "%127s %lld", lv_name, &lv_size) != 2) continue;
        
        // Skip target LV
        if (strcmp(lv_name, target_lv) == 0) continue;
        
        // Get filesystem type
        char fs_type[32];
        if (get_filesystem_type(vg_name, lv_name, fs_type, sizeof(fs_type)) != 0) {
            LOG_DEBUG("Extender", "Skipping LV '%s/%s' - cannot determine filesystem", 
                     vg_name, lv_name);
            continue;
        }
        
        // Check if filesystem can be shrunk
        if (!can_shrink_filesystem(fs_type)) {
            LOG_DEBUG("Extender", "Skipping LV '%s/%s' - %s cannot be shrunk",
                     vg_name, lv_name, fs_type);
            continue;
        }
        
        // Check free space in filesystem
        long long fs_free = get_fs_free_space(vg_name, lv_name);
        if (fs_free < min_free_bytes) {
            LOG_DEBUG("Extender", "Skipping LV '%s/%s' - insufficient free space (%lld bytes)",
                     vg_name, lv_name, fs_free);
            continue;
        }
        
        // Found a suitable donor!
        donors_found++;
        char size_str[64];
        format_bytes(shrink_size, size_str, sizeof(size_str));
        
        LOG_INFO("Extender", "Found donor LV: %s/%s (will shrink by %s)", 
                 vg_name, lv_name, size_str);
        
        // Execute shrink command
        snprintf(cmd, sizeof(cmd),
                 "sudo lvreduce -r -L -%dG /dev/%s/%s -y 2>&1",
                 EXTEND_SIZE_GB, vg_name, lv_name);
        
        char desc[256];
        snprintf(desc, sizeof(desc), "Shrink %s/%s by %dGB", vg_name, lv_name, EXTEND_SIZE_GB);
        
        if (execute_lvm_command(cmd, desc) == 0) {
            bytes_freed += shrink_size;
            stats_increment_shrink();
            LOG_SUCCESS("Extender", "Successfully shrunk %s/%s", vg_name, lv_name);
            
            // Check if we've freed enough
            if (bytes_freed >= needed_bytes) {
                break;
            }
        }
    }
    
    pclose(fp);
    
    if (donors_found == 0) {
        LOG_WARN("Extender", "No suitable donor LVs found in VG '%s'", vg_name);
    } else {
        char freed_str[64];
        format_bytes(bytes_freed, freed_str, sizeof(freed_str));
        LOG_INFO("Extender", "Total space freed from donors: %s", freed_str);
    }
    
    return bytes_freed;
}

// ─────────────────────────────────────────────────────
// EXTEND LV
// ─────────────────────────────────────────────────────
int extend_lv(const char *vg_name, const char *lv_name, long long size_bytes) {
    char cmd[MAX_COMMAND_LEN];
    char size_str[64];
    
    format_bytes(size_bytes, size_str, sizeof(size_str));
    LOG_INFO("Extender", "Extending LV %s/%s by %s", vg_name, lv_name, size_str);
    
    // Build command
    snprintf(cmd, sizeof(cmd),
             "sudo lvextend -r -L +%dG /dev/%s/%s 2>&1",
             EXTEND_SIZE_GB, vg_name, lv_name);
    
    char desc[256];
    snprintf(desc, sizeof(desc), "Extend %s/%s by %dGB", vg_name, lv_name, EXTEND_SIZE_GB);
    
    int ret = execute_lvm_command(cmd, desc);
    
    if (ret == 0) {
        stats_increment_extension_success();
    } else {
        stats_increment_extension_fail();
    }
    
    return ret;
}

// ─────────────────────────────────────────────────────
// ADD FALLBACK PV
// ─────────────────────────────────────────────────────
int add_fallback_pv(const char *vg_name, const char *fallback_device) {
    char cmd[MAX_COMMAND_LEN];
    char desc[256];
    
    // Check if device exists
    if (!file_exists(fallback_device)) {
        LOG_ERROR("Extender", "Fallback device '%s' does not exist", fallback_device);
        return -1;
    }
    
    // Check if already a PV
    if (is_physical_volume(fallback_device)) {
        LOG_WARN("Extender", "Device '%s' is already a physical volume", fallback_device);
        return -1;
    }
    
    LOG_INFO("Extender", "Adding fallback PV '%s' to VG '%s'", fallback_device, vg_name);
    
    // Create PV
    snprintf(cmd, sizeof(cmd), "sudo pvcreate -y %s 2>&1", fallback_device);
    snprintf(desc, sizeof(desc), "Create PV on %s", fallback_device);
    
    if (execute_lvm_command(cmd, desc) != 0) {
        return -1;
    }
    
    // Extend VG
    snprintf(cmd, sizeof(cmd), "sudo vgextend %s %s 2>&1", vg_name, fallback_device);
    snprintf(desc, sizeof(desc), "Extend VG %s with %s", vg_name, fallback_device);
    
    if (execute_lvm_command(cmd, desc) != 0) {
        return -1;
    }
    
    stats_increment_fallback_pv();
    LOG_SUCCESS("Extender", "Successfully added fallback PV '%s' to VG '%s'",
                fallback_device, vg_name);
    
    return 0;
}

// ─────────────────────────────────────────────────────
// MAIN EXTENDER LOGIC
// ─────────────────────────────────────────────────────
int try_extender_for_device(const char *device) {
    char vg_name[128] = "";
    char lv_name[128] = "";
    long long needed_bytes = (long long)EXTEND_SIZE_GB * 1024 * 1024 * 1024;
    
    print_separator();
    LOG_INFO("Extender", "Processing extension request for: %s", device);
    
    // Step 1: Get VG and LV names
    if (get_vg_lv(device, vg_name, sizeof(vg_name), lv_name, sizeof(lv_name)) != 0) {
        LOG_ERROR("Extender", "Could not determine VG/LV for device '%s'", device);
        return -1;
    }
    
    LOG_INFO("Extender", "Target: VG='%s', LV='%s'", vg_name, lv_name);
    
    // Step 2: Check current VG free space
    long long vg_free = get_vg_free_space(vg_name);
    if (vg_free < 0) {
        LOG_ERROR("Extender", "Failed to get free space for VG '%s'", vg_name);
        return -1;
    }
    
    char free_str[64];
    format_bytes(vg_free, free_str, sizeof(free_str));
    LOG_INFO("Extender", "VG '%s' current free space: %s", vg_name, free_str);
    
    // Step 3: Try to free space from donor LVs if needed
    if (vg_free < needed_bytes) {
        LOG_INFO("Extender", "Insufficient VG free space, attempting to shrink donors...");
        long long freed = shrink_donor_lvs(vg_name, lv_name, needed_bytes - vg_free);
        
        // Re-check VG free space
        vg_free = get_vg_free_space(vg_name);
        format_bytes(vg_free, free_str, sizeof(free_str));
        LOG_INFO("Extender", "VG '%s' free space after shrinking: %s", vg_name, free_str);
    }
    
    // Step 4: Try fallback PV if still not enough space
    if (vg_free < needed_bytes) {
        LOG_WARN("Extender", "Still insufficient space, trying fallback PV...");
        
        if (add_fallback_pv(vg_name, FALLBACK_DEV) == 0) {
            // Re-check VG free space
            vg_free = get_vg_free_space(vg_name);
            format_bytes(vg_free, free_str, sizeof(free_str));
            LOG_INFO("Extender", "VG '%s' free space after fallback: %s", vg_name, free_str);
        }
    }
    
    // Step 5: Extend the target LV if enough space
    if (vg_free >= needed_bytes) {
        int ret = extend_lv(vg_name, lv_name, needed_bytes);
        
        if (ret == 0) {
            print_operation_result(1, "Extension Complete",
                                  "Successfully extended logical volume");
            return 0;
        } else {
            print_operation_result(0, "Extension Failed",
                                  "LVM command failed");
            return -1;
        }
    } else {
        LOG_ERROR("Extender", "Cannot extend: insufficient space even after all attempts");
        print_operation_result(0, "Extension Failed",
                             "Insufficient space in volume group");
        return -1;
    }
}
