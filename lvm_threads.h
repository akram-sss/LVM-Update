#ifndef LVM_THREADS_H
#define LVM_THREADS_H

#include <pthread.h>

// ─────────────────────────────────────────────────────
// THREAD FUNCTIONS
// ─────────────────────────────────────────────────────

// Supervisor thread - monitors filesystems and classifies LVs
void* supervisor_thread(void *arg);

// Extender thread - processes extension requests
void* extender_thread(void *arg);

// Writer thread - generates load for testing
void* writer_thread(void *arg);

// HTTP dashboard thread - serves status via HTTP
void* http_thread(void *arg);

// ─────────────────────────────────────────────────────
// QUEUE MANAGEMENT
// ─────────────────────────────────────────────────────

// Enqueue device for extension
void enqueue_device(const char *device);

#endif // LVM_THREADS_H
