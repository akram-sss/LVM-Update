#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include "lvm_utils.h"
#include "lvm_logger.h"
#include "lvm_config.h"

// Global state (defined in lvm_main.c)
vol_status_t volumes[MAX_VOLUMES];
int volumes_count = 0;
pthread_mutex_t volumes_mutex = PTHREAD_MUTEX_INITIALIZER;

system_stats_t sys_stats = {0};
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

// ─────────────────────────────────────────────────────
// VOLUME MANAGEMENT
// ─────────────────────────────────────────────────────

vol_status_t* get_or_create_volume(const char *device, const char *mountpoint) {
    pthread_mutex_lock(&volumes_mutex);
    
    // Search for existing volume
    int i;
    for (i = 0; i < volumes_count; i++) {
        if (strcmp(volumes[i].device, device) == 0) break;
    }
    
    // Create new volume if not found
    if (i == volumes_count && volumes_count < MAX_VOLUMES) {
        memset(&volumes[volumes_count], 0, sizeof(vol_status_t));
        strncpy(volumes[volumes_count].device, device,
                sizeof(volumes[volumes_count].device) - 1);
        volumes[volumes_count].first_seen = time(NULL);
        
        if (mountpoint) {
            strncpy(volumes[volumes_count].mountpoint, mountpoint,
                    sizeof(volumes[volumes_count].mountpoint) - 1);
        }
        
        volumes_count++;
        LOG_INFO("VolManager", "Registered new volume: %s @ %s", device, mountpoint);
    }
    
    // Update mountpoint if provided
    if (i < volumes_count) {
        if (mountpoint && strlen(mountpoint)) {
            strncpy(volumes[i].mountpoint, mountpoint,
                    sizeof(volumes[i].mountpoint) - 1);
        }
    }
    
    vol_status_t *res = (i < volumes_count) ? &volumes[i] : NULL;
    pthread_mutex_unlock(&volumes_mutex);
    return res;
}

void update_volume_status(const char *device, const char *mountpoint,
                         int use_pct, const char *msg) {
    vol_status_t *v = get_or_create_volume(device, mountpoint);
    if (!v) return;
    
    pthread_mutex_lock(&volumes_mutex);
    
    v->use_pct = use_pct;
    v->last_action = time(NULL);
    
    if (msg) {
        strncpy(v->last_msg, msg, sizeof(v->last_msg) - 1);
    }
    
    // Update history ring buffer
    v->history[v->history_pos] = use_pct;
    v->history_pos = (v->history_pos + 1) % HISTORY_SAMPLES;
    if (v->history_filled < HISTORY_SAMPLES) {
        v->history_filled++;
    }
    
    pthread_mutex_unlock(&volumes_mutex);
}

lv_state_t classify_lv(vol_status_t *v) {
    int last = v->use_pct;
    
    // Hungry if usage >= threshold
    if (last >= THRESHOLD_PCT) {
        return LV_HUNGRY;
    }
    
    // Over-provisioned if consistently low
    if (v->history_filled == HISTORY_SAMPLES) {
        int all_low = 1;
        for (int i = 0; i < HISTORY_SAMPLES; i++) {
            if (v->history[i] > LOW_PCT) {
                all_low = 0;
                break;
            }
        }
        if (all_low) {
            return LV_OVERPROVISIONED;
        }
    }
    
    return LV_OK;
}

vol_status_t* find_volume_by_device(const char *device) {
    pthread_mutex_lock(&volumes_mutex);
    for (int i = 0; i < volumes_count; i++) {
        if (strcmp(volumes[i].device, device) == 0) {
            pthread_mutex_unlock(&volumes_mutex);
            return &volumes[i];
        }
    }
    pthread_mutex_unlock(&volumes_mutex);
    return NULL;
}

// ─────────────────────────────────────────────────────
// FILESYSTEM SCANNING
// ─────────────────────────────────────────────────────

int should_monitor_mount(const char *mountpoint) {
    const char *mounts[] = {MONITORED_MOUNTS};
    int count = sizeof(mounts) / sizeof(mounts[0]);
    
    for (int i = 0; i < count; i++) {
        if (strcmp(mountpoint, mounts[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int parse_df_and_find_full(char out_devices[][256], char out_mounts[][256],
                           int out_use[], int *out_count) {
    // Use simple df -P to get ALL mounted filesystems, then filter
    const char *cmd = "df -P 2>/dev/null";
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        LOG_ERROR("DFParser", "Failed to execute df command");
        return -1;
    }
    
    char line[1024];
    int count = 0;
    int line_num = 0;
    
    // Skip header
    if (!fgets(line, sizeof(line), fp)) {
        pclose(fp);
        LOG_ERROR("DFParser", "No output from df command");
        return -1;
    }
    
    LOG_DEBUG("DFParser", "Scanning filesystems...");
    
    // Parse each line
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        char dev[256], size[64], used[64], avail[64], usep[16], mount[256];
        
        int matched = sscanf(line, "%255s %63s %63s %63s %15s %255s",
                            dev, size, used, avail, usep, mount);
        
        if (matched >= 6) {
            // Only process devices starting with /dev/
            if (strncmp(dev, "/dev/", 5) == 0) {
                LOG_DEBUG("DFParser", "Found: %s mounted at %s (%s)", dev, mount, usep);
                
                // Parse percentage
                char tmp[16];
                strncpy(tmp, usep, 15);
                tmp[15] = 0;
                char *p = strchr(tmp, '%');
                if (p) *p = 0;
                int pct = atoi(tmp);
                
                strncpy(out_devices[count], dev, 255);
                out_devices[count][255] = 0;
                strncpy(out_mounts[count], mount, 255);
                out_mounts[count][255] = 0;
                out_use[count] = pct;
                count++;
                
                if (count >= *out_count) break;
            }
        }
    }
    
    pclose(fp);
    
    if (count == 0) {
        LOG_WARN("DFParser", "No /dev/ filesystems found - are LVs mounted?");
        LOG_INFO("DFParser", "Expected mounts: /mnt/lv_home, /mnt/lv_data1, /mnt/lv_data2");
    } else {
        LOG_DEBUG("DFParser", "Found %d filesystem(s)", count);
    }
    
    *out_count = count;
    return 0;
}

// ─────────────────────────────────────────────────────
// LVM OPERATIONS
// ─────────────────────────────────────────────────────

int get_vg_lv(const char *device, char *vg, size_t vgsz, char *lv, size_t lvsz) {
    char cmd[512], buf[512];
    vg[0] = lv[0] = 0;
    
    // Try using lvs command first
    snprintf(cmd, sizeof(cmd),
             "lvs --noheadings -o vg_name,lv_name %s 2>/dev/null | tr -s ' '",
             device);
    
    FILE *fp = popen(cmd, "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            char tmp_vg[128], tmp_lv[128];
            if (sscanf(buf, "%127s %127s", tmp_vg, tmp_lv) == 2) {
                strncpy(vg, tmp_vg, vgsz - 1);
                strncpy(lv, tmp_lv, lvsz - 1);
                vg[vgsz-1] = lv[lvsz-1] = 0;
            }
        }
        pclose(fp);
    }
    
    // Fallback: parse device path (e.g., /dev/mapper/vgdata-lv_home or /dev/vgdata/lv_home)
    if (!vg[0] || !lv[0]) {
        if (strstr(device, "/mapper/")) {
            // Format: /dev/mapper/vg-lv
            const char *name = strrchr(device, '/');
            if (name) {
                name++; // skip '/'
                const char *dash = strchr(name, '-');
                if (dash) {
                    size_t vg_len = dash - name;
                    if (vg_len < vgsz) {
                        strncpy(vg, name, vg_len);
                        vg[vg_len] = 0;
                    }
                    strncpy(lv, dash + 1, lvsz - 1);
                    lv[lvsz-1] = 0;
                }
            }
        } else {
            // Format: /dev/vg/lv
            if (sscanf(device, "/dev/%127[^/]/%127s", vg, lv) != 2) {
                return -1;
            }
        }
    }
    
    return (vg[0] && lv[0]) ? 0 : -1;
}

long long get_vg_free_space(const char *vg_name) {
    char cmd[256], buf[128];
    
    snprintf(cmd, sizeof(cmd),
             "vgs --noheadings --units b --nosuffix -o vg_free %s 2>/dev/null | tr -d ' '",
             vg_name);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    
    long long free_bytes = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        free_bytes = atoll(buf);
    }
    
    pclose(fp);
    return free_bytes;
}

int get_filesystem_type(const char *vg, const char *lv, char *fs_type, size_t fs_size) {
    char cmd[256], buf[64];
    
    snprintf(cmd, sizeof(cmd), "lsblk -no FSTYPE /dev/%s/%s 2>/dev/null", vg, lv);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    
    fs_type[0] = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        // Remove newline
        buf[strcspn(buf, "\n")] = 0;
        strncpy(fs_type, buf, fs_size - 1);
        fs_type[fs_size-1] = 0;
    }
    
    pclose(fp);
    return fs_type[0] ? 0 : -1;
}

long long get_fs_free_space(const char *vg, const char *lv) {
    char cmd[256], buf[128];
    
    snprintf(cmd, sizeof(cmd),
             "df -P --block-size=1 /dev/%s/%s 2>/dev/null | tail -1 | awk '{print $4}'",
             vg, lv);
    
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    
    long long free_bytes = 0;
    if (fgets(buf, sizeof(buf), fp)) {
        free_bytes = atoll(buf);
    }
    
    pclose(fp);
    return free_bytes;
}

int can_shrink_filesystem(const char *fs_type) {
    // ext4 can be shrunk safely with resize2fs
    // XFS CANNOT be shrunk (only grown)
    if (strcmp(fs_type, "ext4") == 0) return 1;
    if (strcmp(fs_type, "ext3") == 0) return 1;
    if (strcmp(fs_type, "ext2") == 0) return 1;
    return 0;
}

int is_physical_volume(const char *device) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "pvs --noheadings -o pv_name 2>/dev/null | grep -w '%s' >/dev/null 2>&1",
             device);
    return (system(cmd) == 0);
}

// ─────────────────────────────────────────────────────
// STATISTICS
// ─────────────────────────────────────────────────────

void stats_increment_checks(void) {
    pthread_mutex_lock(&stats_mutex);
    sys_stats.checks_performed++;
    sys_stats.last_check = time(NULL);
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_extension_success(void) {
    pthread_mutex_lock(&stats_mutex);
    sys_stats.extensions_succeeded++;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_extension_fail(void) {
    pthread_mutex_lock(&stats_mutex);
    sys_stats.extensions_failed++;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_shrink(void) {
    pthread_mutex_lock(&stats_mutex);
    sys_stats.shrinks_performed++;
    pthread_mutex_unlock(&stats_mutex);
}

void stats_increment_fallback_pv(void) {
    pthread_mutex_lock(&stats_mutex);
    sys_stats.fallback_pvs_added++;
    pthread_mutex_unlock(&stats_mutex);
}

// ─────────────────────────────────────────────────────
// UTILITIES
// ─────────────────────────────────────────────────────

int execute_command(const char *cmd, char *output, size_t output_size) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;
    
    if (output && output_size > 0) {
        output[0] = 0;
        if (fgets(output, output_size, fp)) {
            output[strcspn(output, "\n")] = 0;
        }
    }
    
    int status = pclose(fp);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

void format_bytes(long long bytes, char *output, size_t size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double value = (double)bytes;
    
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    
    snprintf(output, size, "%.2f %s", value, units[unit]);
}

int file_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0);
}
