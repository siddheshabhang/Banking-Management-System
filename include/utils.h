#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * Utilities for file-backed record storage and locking.
 *
 * Note: These helpers provide an initial safe interface; you will refine
 * them to implement ACID-like properties: use POSIX advisory locks (fcntl),
 * atomic renames for writes, journaling if desired, and careful concurrency.
 */

/* Open file for read/write creating if necessary (returns fd or -1) */
int open_db_file(const char *path, int flags, mode_t mode);

/* Acquire an advisory lock on a file descriptor.
 * - exclusive == 1 for write lock, 0 for shared/read lock
 * - blocking == 1 to block until lock obtained, 0 to return immediately
 * Returns 0 on success, -1 on failure (errno set).
 */
int fd_lock(int fd, int exclusive, int blocking);

/* Release advisory lock */
int fd_unlock(int fd);

/* Helper to write a complete buffer to fd (handles partial writes) */
ssize_t write_all(int fd, const void *buf, size_t count);

/* Helper to read an exact count from fd, handling partial reads.
 * Returns number of bytes read (==count) or -1 on error, 0 on EOF.
 */
ssize_t read_all(int fd, void *buf, size_t count);

/* Helpers to allocate monotonic unique IDs (thread-safe).
 * You can implement them by reading/writing a simple counter file or by
 * using time + randomness for initial versions.
 */
uint64_t generate_unique_id(const char *counter_file);

/* Password hashing helpers (implementations may use a library or simple SHA256).
 * - hash_password: produce a text representation of the hash into out_hash (size must be >= MAX_PASSWORD_LEN)
 * - verify_password: checks plain password against stored hash.
 * For project use you can implement a simple salted SHA256 using OpenSSL or
 * fallback to a simple custom hash (not for production).
 */
int hash_password(const char *password, const char *salt, char *out_hash, size_t out_sz);
int verify_password(const char *password, const char *salt, const char *stored_hash);

/* Convenience: safe string copy that zero-terminates */
void safe_strncpy(char *dst, const char *src, size_t dst_sz);

/* Helper to ensure directory exists (recursively if needed) */
int mkdir_p(const char *path);

/* Logging helper (thread-safe) */
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif /* UTILS_H */
