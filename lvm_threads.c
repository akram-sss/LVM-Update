#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "lvm_threads.h"
#include "lvm_logger.h"
#include "lvm_utils.h"
#include "lvm_extender.h"
#include "lvm_config.h"

// Global state (extern declarations)
extern char pending_device[256];
extern pthread_mutex_t pending_mutex;
extern pthread_cond_t pending_cond;
extern volatile int shutdown_requested;

// ─────────────────────────────────────────────────────
// QUEUE MANAGEMENT
// ─────────────────────────────────────────────────────
void enqueue_device(const char *device) {
    pthread_mutex_lock(&pending_mutex);
    
    if (strlen(pending_device) == 0) {
        strncpy(pending_device, device, sizeof(pending_device) - 1);
        pending_device[sizeof(pending_device)-1] = 0;
        pthread_cond_signal(&pending_cond);
        LOG_INFO("Queue", "Enqueued device for extension: %s", device);
    } else {
        LOG_WARN("Queue", "Queue full, skipping enqueue of %s", device);
    }
    
    pthread_mutex_unlock(&pending_mutex);
}

// ─────────────────────────────────────────────────────
// SUPERVISOR THREAD
// ─────────────────────────────────────────────────────
void* supervisor_thread(void *arg) {
    (void)arg;
    
    LOG_INFO("Supervisor", "Thread started - monitoring filesystems");
    
    while (!shutdown_requested) {
        char devs[8][256];
        char mounts[8][256];
        int uses[8];
        int devcount = 8;
        
        // Parse filesystem usage
        if (parse_df_and_find_full(devs, mounts, uses, &devcount) != 0) {
            LOG_ERROR("Supervisor", "Failed to parse filesystem usage");
            sleep(CHECK_INTERVAL);
            continue;
        }
        
        stats_increment_checks();
        LOG_DEBUG("Supervisor", "Checking %d filesystems...", devcount);
        
        // Analyze each volume
        for (int i = 0; i < devcount; i++) {
            // Only monitor configured mount points
            if (!should_monitor_mount(mounts[i])) {
                continue;
            }
            
            // Update volume status
            vol_status_t *v = get_or_create_volume(devs[i], mounts[i]);
            update_volume_status(devs[i], mounts[i], uses[i], "monitored");
            
            // Classify volume state
            lv_state_t state = classify_lv(v);
            
            if (state == LV_HUNGRY) {
                LOG_WARN("Supervisor", "🔥 HUNGRY LV: %s at %s (%d%%) - needs extension",
                        devs[i], mounts[i], uses[i]);
                
                update_volume_status(devs[i], mounts[i], uses[i],
                                   "queued for extension");
                enqueue_device(devs[i]);
                
            } else if (state == LV_OVERPROVISIONED) {
                LOG_INFO("Supervisor", "💤 OVER-PROVISIONED LV: %s at %s (%d%%) - donor candidate",
                        devs[i], mounts[i], uses[i]);
                
                update_volume_status(devs[i], mounts[i], uses[i],
                                   "over-provisioned");
                
            } else {
                // LV_OK - normal state
                LOG_DEBUG("Supervisor", "✓ OK: %s at %s (%d%%)",
                         devs[i], mounts[i], uses[i]);
            }
        }
        
        sleep(CHECK_INTERVAL);
    }
    
    LOG_INFO("Supervisor", "Thread shutting down");
    return NULL;
}

// ─────────────────────────────────────────────────────
// EXTENDER THREAD
// ─────────────────────────────────────────────────────
void* extender_thread(void *arg) {
    (void)arg;
    
    LOG_INFO("Extender", "Thread started - ready to process extension requests");
    
    while (!shutdown_requested) {
        // Wait for pending device
        pthread_mutex_lock(&pending_mutex);
        
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 1; // 1 second timeout
        
        while (strlen(pending_device) == 0 && !shutdown_requested) {
            pthread_cond_timedwait(&pending_cond, &pending_mutex, &ts);
        }
        
        if (shutdown_requested) {
            pthread_mutex_unlock(&pending_mutex);
            break;
        }
        
        char device_to_handle[256];
        strncpy(device_to_handle, pending_device, sizeof(device_to_handle) - 1);
        device_to_handle[sizeof(device_to_handle)-1] = 0;
        pending_device[0] = 0;
        
        pthread_mutex_unlock(&pending_mutex);
        
        // Acquire lock to prevent concurrent operations
        int fd = open(LOCK_FILE, O_CREAT | O_RDWR, 0666);
        if (fd < 0) {
            LOG_ERROR("Extender", "Could not open lock file: %s", LOCK_FILE);
            continue;
        }
        
        if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
            LOG_WARN("Extender", "Another extender is running, skipping");
            close(fd);
            continue;
        }
        
        // Process extension
        LOG_INFO("Extender", "🔧 Processing extension for: %s", device_to_handle);
        
        update_volume_status(device_to_handle, "", THRESHOLD_PCT, "extending...");
        
        int rc = try_extender_for_device(device_to_handle);
        
        if (rc == 0) {
            update_volume_status(device_to_handle, "", 0, "extension succeeded");
            LOG_SUCCESS("Extender", "✓ Extension completed successfully");
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "extension failed (code %d)", rc);
            update_volume_status(device_to_handle, "", 0, msg);
            LOG_ERROR("Extender", "✗ Extension failed with code %d", rc);
        }
        
        // Release lock
        flock(fd, LOCK_UN);
        close(fd);
        
        // Brief pause before next operation
        sleep(3);
    }
    
    LOG_INFO("Extender", "Thread shutting down");
    return NULL;
}

// ─────────────────────────────────────────────────────
// WRITER THREAD (Load Generator)
// ─────────────────────────────────────────────────────
void* writer_thread(void *arg) {
    const char *writer_name = (const char *)arg;
    char workdir[512];
    
    snprintf(workdir, sizeof(workdir), "%s/%s", WRITER_BASE_PATH, writer_name);
    
    if (DRY_RUN) {
        LOG_INFO(writer_name, "Started in DRY-RUN mode (simulating writes to %s)", workdir);
    } else {
        mkdir(workdir, 0755);
        LOG_INFO(writer_name, "Started - writing to %s", workdir);
    }
    
    unsigned long i = 0;
    
    while (!shutdown_requested) {
        i++;
        char fname[1024];
        snprintf(fname, sizeof(fname), "%s/%s_file_%lu.dat", workdir, writer_name, i);
        
        if (DRY_RUN) {
            if (i % 100 == 0) {
                LOG_DEBUG(writer_name, "[DRY-RUN] Would create file #%lu", i);
            }
            usleep(50000);
        } else {
            char cmd[1024];
            snprintf(cmd, sizeof(cmd),
                     "dd if=/dev/zero of='%s' bs=1M count=%d conv=fsync >/dev/null 2>&1",
                     fname, WRITER_FILE_SIZE_MB);
            system(cmd);
            
            // Periodic cleanup to avoid filling up
            if (i % 500 == 0) {
                char cleanup[1024];
                snprintf(cleanup, sizeof(cleanup),
                         "ls -1t %s 2>/dev/null | tail -n +201 | xargs -r -I{} rm -f %s/{}",
                         workdir, workdir);
                system(cleanup);
                LOG_DEBUG(writer_name, "Cleaned old files (kept latest 200)");
            }
            
            usleep(WRITER_SLEEP_USEC);
        }
    }
    
    LOG_INFO(writer_name, "Thread shutting down (wrote %lu files)", i);
    return NULL;
}

// ─────────────────────────────────────────────────────
// HTTP DASHBOARD THREAD
// ─────────────────────────────────────────────────────
void* http_thread(void *arg) {
    (void)arg;
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG_ERROR("HTTP", "Failed to create socket");
        return NULL;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DASHBOARD_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("HTTP", "Failed to bind to port %d", DASHBOARD_PORT);
        close(server_fd);
        return NULL;
    }
    
    listen(server_fd, 4);
    LOG_SUCCESS("HTTP", "Dashboard listening on http://0.0.0.0:%d", DASHBOARD_PORT);
    
    while (!shutdown_requested) {
        struct sockaddr_in client;
        socklen_t cl = sizeof(client);
        
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(server_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity <= 0) continue;
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client, &cl);
        if (client_fd < 0) continue;
        
        // Build JSON response
        extern system_stats_t sys_stats;
        extern vol_status_t volumes[];
        extern int volumes_count;
        
        char json[MAX_BUFFER_LEN];
        int off = 0;
        
        off += snprintf(json + off, sizeof(json) - off,
                       "{\"status\":\"running\",\"dry_run\":%s,\"stats\":{",
                       DRY_RUN ? "true" : "false");
        
        pthread_mutex_lock(&stats_mutex);
        off += snprintf(json + off, sizeof(json) - off,
                       "\"checks\":%lu,\"extensions_ok\":%lu,\"extensions_fail\":%lu,"
                       "\"shrinks\":%lu,\"fallback_pvs\":%lu",
                       sys_stats.checks_performed, sys_stats.extensions_succeeded,
                       sys_stats.extensions_failed, sys_stats.shrinks_performed,
                       sys_stats.fallback_pvs_added);
        pthread_mutex_unlock(&stats_mutex);
        
        off += snprintf(json + off, sizeof(json) - off, "},\"volumes\":[");
        
        pthread_mutex_lock(&volumes_mutex);
        for (int i = 0; i < volumes_count; i++) {
            off += snprintf(json + off, sizeof(json) - off,
                           (i == 0) ? "{\"device\":\"%s\",\"mount\":\"%s\",\"use\":%d,\"msg\":\"%s\"}"
                                    : ",{\"device\":\"%s\",\"mount\":\"%s\",\"use\":%d,\"msg\":\"%s\"}",
                           volumes[i].device, volumes[i].mountpoint,
                           volumes[i].use_pct, volumes[i].last_msg);
        }
        pthread_mutex_unlock(&volumes_mutex);
        
        off += snprintf(json + off, sizeof(json) - off, "]}");
        
        // Send response
        char response[MAX_BUFFER_LEN + 256];
        int resp_len = snprintf(response, sizeof(response),
                               "HTTP/1.1 200 OK\r\n"
                               "Content-Type: application/json\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Content-Length: %d\r\n\r\n%s",
                               (int)strlen(json), json);
        
        send(client_fd, response, resp_len, 0);
        close(client_fd);
        
        LOG_DEBUG("HTTP", "Served dashboard request");
    }
    
    close(server_fd);
    LOG_INFO("HTTP", "Thread shutting down");
    return NULL;
}
