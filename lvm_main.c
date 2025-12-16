#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "lvm_config.h"
#include "lvm_types.h"
#include "lvm_logger.h"
#include "lvm_utils.h"
#include "lvm_threads.h"

// ─────────────────────────────────────────────────────
// GLOBAL STATE
// ─────────────────────────────────────────────────────
char pending_device[256] = {0};
pthread_mutex_t pending_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pending_cond = PTHREAD_COND_INITIALIZER;

volatile int shutdown_requested = 0;

// ─────────────────────────────────────────────────────
// SIGNAL HANDLERS
// ─────────────────────────────────────────────────────
static void signal_handler(int signum) {
    const char *sig_name = "UNKNOWN";
    
    switch (signum) {
        case SIGINT:  sig_name = "SIGINT (Ctrl+C)"; break;
        case SIGTERM: sig_name = "SIGTERM"; break;
        case SIGHUP:  sig_name = "SIGHUP"; break;
    }
    
    LOG_WARN("Main", "Received signal %s - initiating graceful shutdown...", sig_name);
    shutdown_requested = 1;
    
    // Wake up waiting threads
    pthread_cond_broadcast(&pending_cond);
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    LOG_DEBUG("Main", "Signal handlers installed");
}

// ─────────────────────────────────────────────────────
// MAIN PROGRAM
// ─────────────────────────────────────────────────────
int main(int argc, char **argv) {
    // Initialize logger
    log_init();
    
    // Print banner
    print_banner();
    
    // Initialize statistics
    sys_stats.start_time = time(NULL);
    
    // Print configuration
    print_config_summary();
    
    // Setup signal handlers
    setup_signal_handlers();
    
    if (DRY_RUN) {
        LOG_WARN("Main", "⚠️  DRY-RUN MODE ENABLED - No real LVM operations will be performed");
        LOG_WARN("Main", "⚠️  To enable real operations, set DRY_RUN=0 in lvm_config.h");
    } else {
        LOG_WARN("Main", "⚡ PRODUCTION MODE - Real LVM operations will be executed");
    }
    
    print_separator();
    
    // Create threads
    pthread_t threads[10];
    int thread_count = 0;
    
    LOG_INFO("Main", "Starting worker threads...");
    
    // Supervisor thread
    if (pthread_create(&threads[thread_count++], NULL, supervisor_thread, NULL) != 0) {
        LOG_CRITICAL("Main", "Failed to create supervisor thread");
        return 1;
    }
    
    // Extender thread
    if (pthread_create(&threads[thread_count++], NULL, extender_thread, NULL) != 0) {
        LOG_CRITICAL("Main", "Failed to create extender thread");
        return 1;
    }
    
    // HTTP dashboard thread (if enabled)
    if (DASHBOARD_ENABLED) {
        if (pthread_create(&threads[thread_count++], NULL, http_thread, NULL) != 0) {
            LOG_ERROR("Main", "Failed to create HTTP thread (continuing without dashboard)");
        }
    }
    
    // Writer threads (if enabled)
    if (WRITER_ENABLED) {
        static const char *writer_names[WRITER_COUNT] = {"writer1", "writer2"};
        
        for (int i = 0; i < WRITER_COUNT; i++) {
            if (pthread_create(&threads[thread_count++], NULL, writer_thread,
                              (void*)writer_names[i]) != 0) {
                LOG_ERROR("Main", "Failed to create writer thread %d", i+1);
            }
        }
    }
    
    LOG_SUCCESS("Main", "All threads started successfully");
    print_separator();
    
    if (DASHBOARD_ENABLED) {
        LOG_INFO("Main", "📊 Dashboard: http://localhost:%d", DASHBOARD_PORT);
    }
    
    LOG_INFO("Main", "📈 Monitoring interval: %d seconds", CHECK_INTERVAL);
    LOG_INFO("Main", "🔥 Hungry threshold: >=%d%%", THRESHOLD_PCT);
    LOG_INFO("Main", "💤 Low threshold: <%d%%", LOW_PCT);
    
    print_separator();
    LOG_INFO("Main", "✓ System operational - Press Ctrl+C to stop");
    print_separator();
    
    // Main loop - wait for shutdown signal
    while (!shutdown_requested) {
        sleep(5);
        
        // Periodic statistics display (every 60 seconds)
        static time_t last_stats_print = 0;
        time_t now = time(NULL);
        
        if (now - last_stats_print >= 60) {
            print_separator();
            print_statistics();
            print_separator();
            last_stats_print = now;
        }
    }
    
    // Shutdown sequence
    print_separator();
    LOG_INFO("Main", "Shutting down - waiting for threads to complete...");
    
    // Wait for all threads to finish
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    print_separator();
    LOG_SUCCESS("Main", "All threads stopped");
    
    // Print final statistics
    print_statistics();
    
    // Cleanup
    pthread_mutex_destroy(&volumes_mutex);
    pthread_mutex_destroy(&pending_mutex);
    pthread_mutex_destroy(&stats_mutex);
    pthread_cond_destroy(&pending_cond);
    
    print_separator();
    LOG_SUCCESS("Main", "Shutdown complete");
    printf("\n");
    
    return 0;
}
