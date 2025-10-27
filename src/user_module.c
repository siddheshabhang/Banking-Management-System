/*
 * src/user_module.c
 * Handles user authentication and management
 * Uses users.db binary file for persistent storage.
 * Compatible with macOS (POSIX fcntl locks)
 */

#include "include/server.h"
#include "include/utils.h"

#define USER_DB_FILE USERS_DB_FILE

/* ---------- Internal helper prototypes ---------- */
static off_t find_user_offset_by_username(int fd, const char *username, user_rec_t *out_user);
static int write_user_record_at(int fd, off_t offset, const user_rec_t *user);

/* ---------- user_login ---------- */
int user_login(const char *username, const char *password, user_rec_t *out_user) {
    if (!username || !password || !out_user)
        return -1;

    int fd = open_db_file(USER_DB_FILE, O_RDONLY | O_CREAT, 0644);
    if (fd < 0) {
        log_error("Failed to open %s", USER_DB_FILE);
        return -1;
    }

    if (fd_lock(fd, 0, 1) < 0) {
        close(fd);
        return -1;
    }

    user_rec_t tmp;
    ssize_t n;
    int found = 0;

    while ((n = read(fd, &tmp, sizeof(tmp))) == sizeof(tmp)) {
        if (strcmp(tmp.username, username) == 0 && tmp.active == STATUS_ACTIVE) {
            if (verify_password(password, tmp.username, tmp.password_hash)) {
                memcpy(out_user, &tmp, sizeof(tmp));
                found = 1;
            }
            break;
        }
    }

    fd_unlock(fd);
    close(fd);

    if (!found) {
        log_error("Invalid credentials for user '%s'", username);
        return -1;
    }

    return 0;
}

/* ---------- user_register ---------- */
int user_register(const char *username, const char *password, const char *name,
                  uint8_t age, const char *address, role_t role) {
    if (!username || !password) return -1;

    ensure_db_dir_exists();
    int fd = open_db_file(USER_DB_FILE, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        log_error("Failed to open user DB");
        return -1;
    }

    if (fd_lock(fd, 1, 1) < 0) {
        close(fd);
        return -1;
    }

    /* Check for existing username */
    user_rec_t tmp;
    while (read(fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
        if (strcmp(tmp.username, username) == 0) {
            fd_unlock(fd);
            close(fd);
            log_error("User '%s' already exists", username);
            return -2;
        }
    }

    user_rec_t new_user;
    memset(&new_user, 0, sizeof(new_user));
    new_user.user_id = (uint32_t)generate_unique_id(DB_DIR"/user_id.counter");
    strncpy(new_user.username, username, sizeof(new_user.username) - 1);
    hash_password(password, username, new_user.password_hash, sizeof(new_user.password_hash));
    strncpy(new_user.name, name ? name : "N/A", sizeof(new_user.name) - 1);
    new_user.age = age;
    strncpy(new_user.address, address ? address : "N/A", sizeof(new_user.address) - 1);
    new_user.role = role;
    new_user.active = STATUS_ACTIVE;
    new_user.created_at = time(NULL);

    if (write_all(fd, &new_user, sizeof(new_user)) != sizeof(new_user)) {
        log_error("Failed to write new user record");
        fd_unlock(fd);
        close(fd);
        return -3;
    }

    fd_unlock(fd);
    close(fd);
    log_info("User '%s' created successfully [role=%d, id=%u]", username, role, new_user.user_id);
    return 0;
}

/* ---------- user_change_password ---------- */
int user_change_password(uint32_t user_id, const char *oldpass, const char *newpass) {
    if (!oldpass || !newpass) return -1;

    int fd = open_db_file(USER_DB_FILE, O_RDWR, 0);
    if (fd < 0) return -1;

    if (fd_lock(fd, 1, 1) < 0) {
        close(fd);
        return -1;
    }

    user_rec_t tmp;
    off_t pos = 0;
    ssize_t n;

    while ((n = read(fd, &tmp, sizeof(tmp))) == sizeof(tmp)) {
        if (tmp.user_id == user_id) {
            if (!verify_password(oldpass, tmp.username, tmp.password_hash)) {
                fd_unlock(fd);
                close(fd);
                return -2; // wrong old password
            }

            hash_password(newpass, tmp.username, tmp.password_hash, sizeof(tmp.password_hash));
            if (write_user_record_at(fd, pos, &tmp) < 0) {
                fd_unlock(fd);
                close(fd);
                return -3;
            }

            fd_unlock(fd);
            close(fd);
            log_info("Password changed for user ID %u", user_id);
            return 0;
        }
        pos += sizeof(tmp);
    }

    fd_unlock(fd);
    close(fd);
    return -4; // user not found
}

/* ---------- Helper: find_user_offset_by_username ---------- */
static off_t find_user_offset_by_username(int fd, const char *username, user_rec_t *out_user) {
    if (fd < 0 || !username) return -1;
    user_rec_t tmp;
    off_t pos = 0;

    while (read(fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
        if (strcmp(tmp.username, username) == 0) {
            if (out_user) memcpy(out_user, &tmp, sizeof(tmp));
            return pos;
        }
        pos += sizeof(tmp);
    }
    return -1;
}

/* ---------- Helper: write_user_record_at ---------- */
static int write_user_record_at(int fd, off_t offset, const user_rec_t *user) {
    if (lseek(fd, offset, SEEK_SET) < 0) return -1;
    if (write_all(fd, user, sizeof(*user)) != sizeof(*user)) return -1;
    fsync(fd);
    return 0;
}

/* ---------- load_user_by_id (for later use) ---------- */
int load_user_by_id(uint32_t user_id, user_rec_t *out_user) {
    int fd = open_db_file(USER_DB_FILE, O_RDONLY, 0);
    if (fd < 0) return -1;
    user_rec_t tmp;
    while (read(fd, &tmp, sizeof(tmp)) == sizeof(tmp)) {
        if (tmp.user_id == user_id) {
            memcpy(out_user, &tmp, sizeof(tmp));
            close(fd);
            return 0;
        }
    }
    close(fd);
    return -1;
}

/* ---------- update_user_record (generic helper) ---------- */
int update_user_record(const user_rec_t *user) {
    if (!user) return -1;
    int fd = open_db_file(USER_DB_FILE, O_RDWR, 0);
    if (fd < 0) return -1;
    if (fd_lock(fd, 1, 1) < 0) {
        close(fd);
        return -1;
    }

    user_rec_t tmp;
    off_t pos = 0;
    ssize_t n;

    while ((n = read(fd, &tmp, sizeof(tmp))) == sizeof(tmp)) {
        if (tmp.user_id == user->user_id) {
            if (write_user_record_at(fd, pos, user) < 0) {
                fd_unlock(fd);
                close(fd);
                return -1;
            }
            fd_unlock(fd);
            close(fd);
            return 0;
        }
        pos += sizeof(tmp);
    }

    fd_unlock(fd);
    close(fd);
    return -1;
}

// Baadmein delete karna hai ye comment
// Add to bottom of user_module.c (just for local test)
#ifdef BUILD_STANDALONE_USER
int main() {
    user_register("alice", "1234", "Alice", 25, "Bangalore", ROLE_CUSTOMER);
    user_rec_t u;
    if (user_login("alice", "1234", &u) == 0)
        printf("Login success! ID=%u Role=%d\n", u.user_id, u.role);
    else
        printf("Login failed\n");
    return 0;
}
#endif
