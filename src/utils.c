/*
 * src/utils.c
 *
 * Utilities for DB file handling, locking, IO, ID generation, password hashing,
 * and logging. macOS-first implementation: uses CommonCrypto for SHA-256.
 *
 * Compile:
 *   clang src/*.c -Iinclude -lpthread -o banking_system
 * If your system doesn't have CommonCrypto, you can compile with OpenSSL:
 *   clang src/*.c -Iinclude -lcrypto -lpthread -o banking_system
 *
 */

#include "include/utils.h"
#include "include/server.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>

#ifdef __APPLE__
/* macOS: use CommonCrypto */
#include <CommonCrypto/CommonDigest.h>
#else
/* Fallback: OpenSSL SHA if available */
#include <openssl/sha.h>
#endif

/* ---------- open_db_file ---------- */
int open_db_file(const char *path, int flags, mode_t mode) {
    int fd = open(path, flags, mode);
    return fd;
}

/* ---------- fd_lock / fd_unlock (fcntl advisory locks) ---------- */
int fd_lock(int fd, int exclusive, int blocking) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }

    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = exclusive ? F_WRLCK : F_RDLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; /* lock whole file */

    int cmd = blocking ? F_SETLKW : F_SETLK;
    if (fcntl(fd, cmd, &fl) == -1) {
        return -1;
    }
    return 0;
}

int fd_unlock(int fd) {
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    if (fcntl(fd, F_SETLK, &fl) == -1) {
        return -1;
    }
    return 0;
}

/* ---------- write_all / read_all ---------- */
ssize_t write_all(int fd, const void *buf, size_t count) {
    const uint8_t *p = (const uint8_t*)buf;
    size_t left = count;
    while (left > 0) {
        ssize_t w = write(fd, p, left);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        left -= (size_t)w;
        p += w;
    }
    return (ssize_t)count;
}

ssize_t read_all(int fd, void *buf, size_t count) {
    uint8_t *p = (uint8_t*)buf;
    size_t left = count;
    while (left > 0) {
        ssize_t r = read(fd, p, left);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) {
            /* EOF reached before reading requested bytes */
            return 0;
        }
        left -= (size_t)r;
        p += r;
    }
    return (ssize_t)count;
}

/* ---------- mkdir_p (recursive) ---------- */
int mkdir_p(const char *path) {
    if (!path || path[0] == '\0') return -1;
    char tmp[1024];
    size_t len = strnlen(path, sizeof(tmp)-1);
    if (len == 0 || len >= sizeof(tmp)) return -1;
    strncpy(tmp, path, len);
    tmp[len] = '\0';

    /* Remove trailing slash */
    if (len > 1 && tmp[len-1] == '/') tmp[len-1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) < 0) {
                if (errno != EEXIST) return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) < 0) {
        if (errno != EEXIST) return -1;
    }
    return 0;
}

/* ---------- ensure_db_dir_exists ---------- */
int ensure_db_dir_exists(void) {
    return mkdir_p(DB_DIR);
}

/* ---------- generate_unique_id ---------- */
/*
 * Simple monotonic counter file. Uses fcntl exclusive lock when updating.
 * Format: ASCII decimal number in file.
 */
uint64_t generate_unique_id(const char *counter_file) {
    if (!counter_file) counter_file = DB_DIR"/.counter";
    int fd = open(counter_file, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return 0;

    if (fd_lock(fd, 1, 1) < 0) {
        close(fd);
        return 0;
    }

    /* read existing value */
    char buf[64];
    ssize_t r = pread(fd, buf, sizeof(buf)-1, 0);
    uint64_t cur = 0;
    if (r > 0) {
        buf[r] = '\0';
        /* tolerate newline */
        cur = (uint64_t)strtoull(buf, NULL, 10);
    }
    uint64_t next = (cur == 0) ? 1 : cur + 1;

    /* write new value (truncate then write) */
    char out[64];
    int n = snprintf(out, sizeof(out), "%" PRIu64 "\n", next);
    if (ftruncate(fd, 0) < 0) {
        /* continue but best effort */
    }
    if (pwrite(fd, out, (size_t)n, 0) != n) {
        /* ignore error, but return 0 to indicate failure */
        fd_unlock(fd);
        close(fd);
        return 0;
    }

    fsync(fd);
    fd_unlock(fd);
    close(fd);
    return next;
}

/* ---------- safe_strncpy ---------- */
void safe_strncpy(char *dst, const char *src, size_t dst_sz) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

/* ---------- Logging helpers ---------- */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_info(const char *fmt, ...) {
    pthread_mutex_lock(&log_mutex);
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[INFO] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
    pthread_mutex_unlock(&log_mutex);
}

void log_error(const char *fmt, ...) {
    pthread_mutex_lock(&log_mutex);
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[ERROR] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    pthread_mutex_unlock(&log_mutex);
}

/* ---------- Password hashing / verification ---------- */
/*
 * We will produce a SHA-256 hex digest of (salt || password).
 * The caller provides salt (which can be random or username). The output
 * is textual hex representation; ensure out_sz >= (SHA256_HEX_LEN + 1).
 *
 * This is *not* a slow KDF (like PBKDF2) — acceptable for this educational project.
 */

static void _sha256_hex(const uint8_t *data, size_t data_len, char *out_hex, size_t out_sz) {
    const size_t HASH_LEN = 32;
    uint8_t digest[HASH_LEN];

#ifdef __APPLE__
    CC_SHA256(data, (CC_LONG)data_len, digest);
#else
    SHA256_CTX c;
    SHA256_Init(&c);
    SHA256_Update(&c, data, data_len);
    SHA256_Final(digest, &c);
#endif

    /* hex encode */
    const char hexchars[] = "0123456789abcdef";
    if (out_sz < (HASH_LEN * 2 + 1)) {
        /* not enough space */
        if (out_sz > 0) out_hex[0] = '\0';
        return;
    }
    for (size_t i = 0; i < HASH_LEN; ++i) {
        out_hex[i*2]     = hexchars[(digest[i] >> 4) & 0xF];
        out_hex[i*2 + 1] = hexchars[digest[i] & 0xF];
    }
    out_hex[HASH_LEN * 2] = '\0';
}

/* hash_password: out_hash must be >= MAX_PASSWORD_LEN */
int hash_password(const char *password, const char *salt, char *out_hash, size_t out_sz) {
    if (!password || !salt || !out_hash) return -1;
    size_t plen = strlen(password);
    size_t slen = strlen(salt);
    size_t tot = plen + slen;
    uint8_t *buf = (uint8_t*)malloc(tot);
    if (!buf) return -1;
    memcpy(buf, salt, slen);
    memcpy(buf + slen, password, plen);
    _sha256_hex(buf, tot, out_hash, out_sz);
    free(buf);
    return 0;
}

/* verify_password: recompute and compare in constant-time */
int verify_password(const char *password, const char *salt, const char *stored_hash) {
    if (!password || !salt || !stored_hash) return 0;
    char tmp[MAX_PASSWORD_LEN];
    if (hash_password(password, salt, tmp, sizeof(tmp)) != 0) return 0;
    /* constant-time compare */
    size_t a = strlen(tmp);
    size_t b = strlen(stored_hash);
    if (a != b) return 0;
    unsigned char diff = 0;
    for (size_t i = 0; i < a; ++i) diff |= (tmp[i] ^ stored_hash[i]);
    return diff == 0;
}
