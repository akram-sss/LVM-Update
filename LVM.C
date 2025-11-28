#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>
#include <sys/socket.h>

//! ---------------------------------------------///
// CONFIGURATION (Edit these values only) //
//! ---------------------------------------------///
static int DRY_RUN = 1;                // 1 = dry-run, 0 = real changes
static const int CHECK_INTERVAL = 8;   // seconds between checks
static const int THRESHOLD_PCT = 80;   // usage to trigger help (was 95)
static const int LOW_PCT = 40;         // "over-provisioned" if < LOW_PCT for long
#define HISTORY_SAMPLES 12             // ~12 * 8s = ~96s window

static const char *FALLBACK_DEV = "/dev/sdc"; // fallback PV device
static const char *LOCK_FILE   = "/tmp/lvm_auto_extender.lock";


static const int DASHBOARD_PORT = 8080;

static const char *WRITER_BASE_PATH   = "/mnt/lv_home";
static const int WRITER_FILE_SIZE_MB  = 1;
static const int WRITER_SLEEP_USEC    = 20000;

//! ---------------------------------------------
// INTERNAL STATUS STRUCTURES & LOCKS  //
//! ---------------------------------------------
typedef struct {
    char device[128];
    char mountpoint[256];
    int  use_pct;                      // last seen usage
    time_t last_action;
    char last_msg[512];

    int history[HISTORY_SAMPLES];      // ring buffer of usage history
    int history_pos;
    int history_filled;
} vol_status_t;

#define MAX_VOLUMES 64
static vol_status_t volumes[MAX_VOLUMES];
static int volumes_count = 0;
pthread_mutex_t volumes_mutex = PTHREAD_MUTEX_INITIALIZER;

static char pending_device[256] = {0};
pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pending_cond    = PTHREAD_COND_INITIALIZER;

//! ---------------------------------------------
// UTILS
//! ---------------------------------------------
static void logmsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[%ld] ", time(NULL));
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}


// track/lookup volume by device
static vol_status_t *get_or_create_volume(const char *device,
                                          const char *mountpoint) {
    pthread_mutex_lock(&volumes_mutex);
    int i;
    for (i = 0; i < volumes_count; i++) {
        if (strcmp(volumes[i].device, device) == 0) break;
    }
    if (i == volumes_count && volumes_count < MAX_VOLUMES) {
        memset(&volumes[volumes_count], 0, sizeof(vol_status_t));
        strncpy(volumes[volumes_count].device, device,
                sizeof(volumes[volumes_count].device) - 1);
        if (mountpoint)
            strncpy(volumes[volumes_count].mountpoint, mountpoint,
                    sizeof(volumes[volumes_count].mountpoint) - 1);
        volumes_count++;
    }
    if (i < volumes_count) {
        if (mountpoint && strlen(mountpoint))
            strncpy(volumes[i].mountpoint, mountpoint,
                    sizeof(volumes[i].mountpoint) - 1);
    }
    vol_status_t *res = &volumes[i];
    pthread_mutex_unlock(&volumes_mutex);
    return res;
}

static void update_volume_status(const char *device,
                                 const char *mountpoint,
                                 int use_pct,
                                 const char *msg) {
    vol_status_t *v = get_or_create_volume(device, mountpoint);
    pthread_mutex_lock(&volumes_mutex);
    v->use_pct = use_pct;
    v->last_action = time(NULL);
    if (msg)
        strncpy(v->last_msg, msg, sizeof(v->last_msg) - 1);
    // history
    v->history[v->history_pos] = use_pct;
    v->history_pos = (v->history_pos + 1) % HISTORY_SAMPLES;
    if (v->history_filled < HISTORY_SAMPLES)
        v->history_filled++;
    pthread_mutex_unlock(&volumes_mutex);
}

//! ---------------------------------------------
// CLASSIFICATION
//! ---------------------------------------------
typedef enum { LV_OK, LV_HUNGRY, LV_OVERPROVISIONED } lv_state_t;

static lv_state_t classify_lv(vol_status_t *v) {
    int last = v->use_pct;

    // Hungry if usage >= threshold
    if (last >= THRESHOLD_PCT) return LV_HUNGRY;

    // Over-provisioned if consistently low
    if (v->history_filled == HISTORY_SAMPLES) {
        int all_low = 1;
        for (int i = 0; i < HISTORY_SAMPLES; i++) {
            if (v->history[i] > LOW_PCT) {
                all_low = 0; break;
            }
        }
        if (all_low) return LV_OVERPROVISIONED;
    }
    return LV_OK;
}

//! ---------------------------------------------
// parse df and find usage
//! ---------------------------------------------
static int parse_df_and_find_full(char out_devices[][256],
                                  char out_mounts[][256],
                                  int out_use[],
                                  int *out_count) {
    FILE *fp = popen("df -P -t xfs -t ext4", "r");
    if (!fp) return -1;
    char line[1024];
    int count = 0;
    fgets(line, sizeof(line), fp); // header
    while (fgets(line, sizeof(line), fp)) {
        char dev[256], size[64], used[64], avail[64], usep[16], mount[256];
        int matched = sscanf(line, "%255s %63s %63s %63s %15s %255s",
                             dev, size, used, avail, usep, mount);
        if (matched >= 6) {
            if (strncmp(dev, "/dev/", 5) == 0) {
                char tmp[16]; strncpy(tmp, usep, 15); tmp[15] = 0;
                char *p = strchr(tmp, '%'); if (p) *p = 0;
                int pct = atoi(tmp);
                strncpy(out_devices[count], dev, 255);
                strncpy(out_mounts[count], mount, 255);
                out_use[count] = pct;
                count++;
                if (count >= *out_count) break;
            }
        }
    }
    pclose(fp);
    *out_count = count;
    return 0;
}

//! ---------------------------------------------
// LVM helpers
//! ---------------------------------------------
static int get_vg_lv(const char *device, char *vg, size_t vgsz,
                     char *lv, size_t lvsz) {
    char cmd[512], buf[512];
    vg[0] = lv[0] = 0;
    snprintf(cmd, sizeof(cmd),
             "lvs --noheadings -o vg_name,lv_name %s 2>/dev/null | tr -s ' '",
             device);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) {
            sscanf(buf, "%127s %127s", vg, lv);
        }
        pclose(fp);
    }
    if (!vg[0] || !lv[0]) {
        if (sscanf(device, "/dev/%127[^/]/%127s", vg, lv) != 2) {
            return -1;
        }
    }
    return 0;
}

//! ---------------------------------------------
// EXTENDER
//! ---------------------------------------------
static int try_extender_for_device(const char *device) {
    char vgname[128] = "", lvname[128] = "";
    char cmd[512], buf[1024];

    // 1️⃣ Get VG/LV names
    if (get_vg_lv(device, vgname, sizeof(vgname), lvname, sizeof(lvname)) != 0) {
        logmsg("Could not determine VG/LV for device %s", device);
        return -1;
    }
    logmsg("Extender: VG=%s LV=%s for device=%s", vgname, lvname, device);

    long long one_gib = 1024LL * 1024 * 1024;

    // 2️⃣ Loop over siblings repeatedly until enough VG free or no more donors
    int changed;
    do {
        changed = 0;

        // Re-scan LVs
        snprintf(cmd, sizeof(cmd),
                 "lvs --noheadings -o lv_name,lv_size,vg_name --units b --nosuffix %s",
                 vgname);
        FILE *fp = popen(cmd, "r");
        if (!fp) break;

        while (fgets(buf, sizeof(buf), fp)) {
            char sib_lv[128]; long long sib_size; char sib_vg[128];
            if (sscanf(buf, "%127s %lld %127s", sib_lv, &sib_size, sib_vg) != 3) continue;
            if (strcmp(sib_lv, lvname) == 0) continue; // skip target LV

            // Only shrink ext4 safely
            snprintf(cmd, sizeof(cmd), "lsblk -no FSTYPE /dev/%s/%s", vgname, sib_lv);
            FILE *fs_fp = popen(cmd, "r");
            if (!fs_fp) continue;
            char fs_type[16]; fs_type[0] = 0;
            if (fgets(fs_type, sizeof(fs_type), fs_fp)) fs_type[strcspn(fs_type, "\n")] = 0;
            pclose(fs_fp);

            if (strcmp(fs_type, "ext4") != 0) continue;

            // Check free space inside ext4
            snprintf(cmd, sizeof(cmd),
                     "df -P --block-size=1 /dev/%s/%s | tail -1 | awk '{print $4}'",
                     vgname, sib_lv);
            FILE *free_fp = popen(cmd, "r");
            long long free_bytes = 0;
            if (free_fp && fgets(buf, sizeof(buf), free_fp)) free_bytes = atoll(buf);
            if (free_fp) pclose(free_fp);

            if (free_bytes >= one_gib) {
                snprintf(cmd, sizeof(cmd),
                         DRY_RUN ? "[DRY-RUN] Would shrink /dev/%s/%s by 1GB"
                                 : "sudo lvreduce -r -L -1G /dev/%s/%s -y",
                         vgname, sib_lv);
                logmsg("%s", cmd);
                if (!DRY_RUN) system(cmd);
                changed = 1;
            }
        }
        pclose(fp);

        // Re-check VG free space after each donor pass
        long long vg_free_bytes = 0;
        snprintf(cmd, sizeof(cmd),
                 "vgs --noheadings --units b --nosuffix -o vg_free %s 2>/dev/null | tr -d ' '",
                 vgname);
        FILE *fp2 = popen(cmd, "r");
        if (fp2) {
            if (fgets(buf, sizeof(buf), fp2)) vg_free_bytes = atoll(buf);
            pclose(fp2);
        }
        logmsg("VG %s free bytes ~= %lld", vgname, vg_free_bytes);
        if (vg_free_bytes >= one_gib) {
            // Enough space now, go extend LV
            snprintf(cmd, sizeof(cmd),
                     DRY_RUN ? "[DRY-RUN] Would extend /dev/%s/%s by 1GB"
                             : "sudo lvextend -r -L +1G /dev/%s/%s",
                     vgname, lvname);
            logmsg("%s", cmd);
            if (!DRY_RUN) system(cmd);
            return 0;
        }
    } while (changed);

    // 3️⃣ Check fallback PVs if VG still has <1GB free
    long long vg_free_bytes = 0;
    snprintf(cmd, sizeof(cmd),
             "vgs --noheadings --units b --nosuffix -o vg_free %s 2>/dev/null | tr -d ' '",
             vgname);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp)) vg_free_bytes = atoll(buf);
        pclose(fp);
    }
    logmsg("VG %s free bytes ~= %lld (before fallback PVs)", vgname, vg_free_bytes);

    const char *fallback_list[] = { FALLBACK_DEV }; // add more as needed
    int first = 1;
    int fallback_count = sizeof(fallback_list) / sizeof(fallback_list[0]);
    for (int i = 0; i < fallback_count; i++) {
        const char *pv = fallback_list[i];
        struct stat st;
        if (stat(pv, &st) != 0) continue;

        // Check if already PV
        snprintf(cmd, sizeof(cmd),
                 "pvs --noheadings -o pv_name | grep -w '%s'", pv);
        int ispv = (system(cmd) == 0);
        if (ispv) {
            logmsg("PV %s already exists, skipping.", pv);
            continue;
        }

        // Ask for user confirmation if not first fallback (you only have one now)
        if (!first) {
            char ans[8];
            printf("Do you want to add PV %s to VG %s? [y/N]: ", pv, vgname);
            if (!fgets(ans, sizeof(ans), stdin)) continue;
            if (ans[0] != 'y' && ans[0] != 'Y') continue;
        }
        first = 0;

        // Create PV
        snprintf(cmd, sizeof(cmd),
                 DRY_RUN ? "[DRY-RUN] Would pvcreate %s" : "sudo pvcreate -y %s", pv);
        logmsg("%s", cmd);
        if (!DRY_RUN) system(cmd);

        // Extend VG
        snprintf(cmd, sizeof(cmd),
                 DRY_RUN ? "[DRY-RUN] Would vgextend %s %s" : "sudo vgextend %s %s",
                 vgname, pv);
        logmsg("%s", cmd);
        if (!DRY_RUN) system(cmd);

        // Re-check VG free space
        snprintf(cmd, sizeof(cmd),
                 "vgs --noheadings --units b --nosuffix -o vg_free %s | tr -d ' '", vgname);
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) vg_free_bytes = atoll(buf);
            pclose(fp);
        }
        if (vg_free_bytes >= one_gib) break; // stop adding PVs if enough
    }

    // 4️⃣ Extend target LV if now enough space after fallback
    if (vg_free_bytes >= one_gib) {
        snprintf(cmd, sizeof(cmd),
                 DRY_RUN ? "[DRY-RUN] Would extend /dev/%s/%s by 1GB"
                         : "sudo lvextend -r -L +1G /dev/%s/%s",
                 vgname, lvname);
        logmsg("%s", cmd);
        if (!DRY_RUN) system(cmd);
        return 0;
    }

    logmsg("Not enough space even after fallback PVs.");
    return -1;
}

//! ---------------------------------------------
// WORKER THREADS
//! ---------------------------------------------
static void *writer_thread(void *arg) {
    const char *writer_name = (const char *)arg;
    char workdir[512];
    snprintf(workdir, sizeof(workdir), "%s/%s", WRITER_BASE_PATH, writer_name);
    if (DRY_RUN) {
        logmsg("[DRY-RUN] Writer %s simulate writing to %s", writer_name, workdir);
    } else {
        mkdir(workdir, 0755);
    }
    unsigned long i = 0;
    while (1) {
        i++;
        char fname[1024];
        snprintf(fname, sizeof(fname),
                 "%s/%s_file_%lu.dat", workdir, writer_name, i);
        if (DRY_RUN) {
            if (i % 100 == 0)
                logmsg("[DRY-RUN] %s would create %s", writer_name, fname);
            usleep(50000);
        } else {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd),
                     "dd if=/dev/zero of='%s' bs=1M count=%d conv=fsync >/dev/null 2>&1",
                     fname, WRITER_FILE_SIZE_MB);
            system(cmd);
            if (i % 500 == 0) {
                char cleanup[1024];
                snprintf(cleanup, sizeof(cleanup),
                         "ls -1t %s | tail -n +201 | xargs -r -I{} rm -f %s/{}",
                         workdir, workdir);
                system(cleanup);
            }
            usleep(WRITER_SLEEP_USEC);
        }
    }
    return NULL;
}

static void enqueue_device(const char *device) {
    pthread_mutex_lock(&pending_mutex);
    if (strlen(pending_device) == 0) {
        strncpy(pending_device, device, sizeof(pending_device) - 1);
        pthread_cond_signal(&pending_cond);
    } else {
        logmsg("Queue full, skipping enqueue of %s", device);
    }
    pthread_mutex_unlock(&pending_mutex);
}

//! ---------------------------------------------
// SUPERVISOR (with simple planner)
//! ---------------------------------------------
static void *supervisor_thread(void *arg) {
    (void)arg;
    while (1) {
        char devs[8][256];
        char mounts[8][256];
        int uses[8];
        int devcount = 8;

        parse_df_and_find_full(devs, mounts, uses, &devcount);

        for (int i = 0; i < devcount; ++i) {
            // Only monitor our lab mounts, ignore OS filesystems
            if (strcmp(mounts[i], "/mnt/lv_home") != 0 &&
                strcmp(mounts[i], "/mnt/lv_data1") != 0 &&
                strcmp(mounts[i], "/mnt/lv_data2") != 0) {
                continue;
            }

            vol_status_t *v = get_or_create_volume(devs[i], mounts[i]);
            update_volume_status(devs[i], mounts[i], uses[i], "updated");
            lv_state_t state = classify_lv(v);

            if (state == LV_HUNGRY) {
                logmsg("Supervisor: HUNGRY LV %s at %s (%d%%)",
                       devs[i], mounts[i], uses[i]);
                update_volume_status(devs[i], mounts[i], uses[i],
                                     "queued for extension");
                enqueue_device(devs[i]);

                char alertbuf[512];
                snprintf(alertbuf, sizeof(alertbuf),
                         "Device %s at %s reached %d%% — queued for extension (DRY_RUN=%d)",
                         devs[i], mounts[i], uses[i], DRY_RUN);
                
            } else if (state == LV_OVERPROVISIONED) {
                logmsg("Supervisor: OVER-PROVISIONED LV %s at %s (%d%%)",
                       devs[i], mounts[i], uses[i]);
            } else {
                // LV_OK: nothing special
            }
        }
        sleep(CHECK_INTERVAL);
    }
    return NULL;
}

//! ---------------------------------------------
// EXTENDER THREAD
//! ---------------------------------------------
static void *extender_thread(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&pending_mutex);
        while (strlen(pending_device) == 0) {
            pthread_cond_wait(&pending_cond, &pending_mutex);
        }
        char device_to_handle[256];
        strncpy(device_to_handle, pending_device,
                sizeof(device_to_handle) - 1);
        device_to_handle[sizeof(device_to_handle)-1] = '\0';
        pending_device[0] = 0;
        pthread_mutex_unlock(&pending_mutex);

        int fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);
        if (fd < 0) {
            logmsg("Could not open lock file");
            continue;
        }
        if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
            logmsg("Another extender running; skipping");
            close(fd);
            continue;
        }

        logmsg("Extender: processing %s", device_to_handle);
        update_volume_status(device_to_handle, "", THRESHOLD_PCT, "extending");
        int rc = try_extender_for_device(device_to_handle);
        if (rc == 0) {
            update_volume_status(device_to_handle, "", 0, "extension succeeded");
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "extension failed code %d", rc);
            update_volume_status(device_to_handle, "", 0, msg);
        }
        flock(fd, LOCK_UN);
        close(fd);
        sleep(3);
    }
    return NULL;
}

//! ---------------------------------------------
// Tiny HTTP Dashboard
//! ---------------------------------------------
static void *http_thread(void *arg) {
    (void)arg;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        logmsg("http: socket failed");
        return NULL;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DASHBOARD_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        logmsg("http: bind failed");
        close(server_fd);
        return NULL;
    }
    listen(server_fd, 4);
    logmsg("HTTP dashboard listening on 0.0.0.0:%d", DASHBOARD_PORT);
    while (1) {
        struct sockaddr_in client;
        socklen_t cl = sizeof(client);
        int c = accept(server_fd, (struct sockaddr *)&client, &cl);
        if (c < 0) continue;

        pthread_mutex_lock(&volumes_mutex);
        char json[8192];
        int off = 0;
        off += snprintf(json + off, sizeof(json) - off, "{ \"volumes\": [");
        for (int i = 0; i < volumes_count; i++) {
            off += snprintf(json + off, sizeof(json) - off,
                            (i == 0)
                            ? "{\"device\":\"%s\",\"mount\":\"%s\",\"use\":%d,\"msg\":\"%s\"}"
                            : ",{\"device\":\"%s\",\"mount\":\"%s\",\"use\":%d,\"msg\":\"%s\"}",
                            volumes[i].device,
                            volumes[i].mountpoint,
                            volumes[i].use_pct,
                            volumes[i].last_msg);
        }
        off += snprintf(json + off, sizeof(json) - off, "]}");
        pthread_mutex_unlock(&volumes_mutex);

        char response[9000];
        int resp_len = snprintf(response, sizeof(response),
                                "HTTP/1.1 200 OK\r\n"
                                "Content-Type: application/json\r\n"
                                "Content-Length: %d\r\n\r\n%s",
                                (int)strlen(json), json);
        send(c, response, resp_len, 0);
        close(c);
    }
    close(server_fd);
    return NULL;
}

//! ---------------------------------------------
// MAIN
//! ---------------------------------------------
int main(int argc, char **argv) {
    printf("LVM Manager starting (DRY_RUN=%d). Threshold=%d%%\n",
           DRY_RUN, THRESHOLD_PCT);
    if (DRY_RUN) {
        logmsg("Running in DRY-RUN mode. No destructive operations will run.");
    }

    pthread_t w1, w2, sup, ext, http;
    pthread_create(&w1, NULL, writer_thread, (void *)"writer1");
    pthread_create(&w2, NULL, writer_thread, (void *)"writer2");
    pthread_create(&sup, NULL, supervisor_thread, NULL);
    pthread_create(&ext, NULL, extender_thread, NULL);
    pthread_create(&http, NULL, http_thread, NULL);

    pthread_join(w1, NULL);
    pthread_join(w2, NULL);
    pthread_join(sup, NULL);
    pthread_join(ext, NULL);
    pthread_join(http, NULL);
    return 0;
}
