#include "utils.h"
#include <sys/file.h>
#include <unistd.h> // For crypt() on macOS // For password hashing! Link with -lcrypt

// Acquire exclusive lock on file
int lock_file(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // lock entire file
    return fcntl(fd, F_SETLKW, &lock);
}

// Unlock file
int unlock_file(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    return fcntl(fd, F_SETLKW, &lock);
}

// --- Hashing Functions (CRITICAL SECURITY FIX) ---

// Generate hash of password
void generate_password_hash(const char *password, char *hash_output, size_t hash_size) {
    // Using SHA-512. The salt should be random and stored with the hash in a real app.
    // For this project, we'll use a simple, constant salt.
    const char *salt = "$6$IIITB$"; // $6$ denotes SHA-512
    
    char *hashed = crypt(password, salt);
    if (hashed) {
        strncpy(hash_output, hashed, hash_size - 1);
        hash_output[hash_size - 1] = '\0';
    } else {
        // Fallback or error
        strncpy(hash_output, "HASH_FAILED", hash_size - 1);
    }
}

// Verify password against stored hash
int verify_password(const char *password, const char *hash) {
    char *verified_hash = crypt(password, hash);
    if (!verified_hash) return 0;
    return (strcmp(verified_hash, hash) == 0);
}

// --- Login Fix (Uses Hashing and consistent type names) ---
// --- Security & Auth (CRITICAL FIX: Enforce Active Status) ---
int login_user(const char *username, const char *password, int *userId, char *role, size_t role_sz) {
    int fd = open(USERS_DB_FILE, O_RDONLY);
    if (fd < 0) return 0;
    
    lock_file(fd);
    user_rec_t user;
    // We use 'found = 1' for SUCCESS, 'found = 2' for INACTIVE, and 'found = 0' for FAILURE.
    int found = 0; 

    while (read(fd, &user, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        if (strcmp(user.username, username) == 0) {
            
            // CRITICAL FIX: Verify using hash
            if (verify_password(password, user.password_hash)) {
                
                // NEW CHECK: Prevent login if account is INACTIVE
                if (user.active == STATUS_INACTIVE) {
                    // Set a flag (2) to indicate successful authentication but inactive status.
                    // The server.c must check for this '2'.
                    found = 2; 
                } else {
                    *userId = user.user_id;
                    
                    // Map role enum to string for client output
                    if (user.role == ROLE_CUSTOMER) strncpy(role, "customer", role_sz);
                    else if (user.role == ROLE_EMPLOYEE) strncpy(role, "employee", role_sz);
                    else if (user.role == ROLE_MANAGER) strncpy(role, "manager", role_sz);
                    else if (user.role == ROLE_ADMIN) strncpy(role, "admin", role_sz);
                    else strncpy(role, "unknown", role_sz);

                    found = 1; // SUCCESS
                }
            }
            break; // Username is unique, so break whether password matched or not
        }
    }
    unlock_file(fd);
    close(fd);
    return found;
}

// --- Concurrency FIX: Atomic Read-Modify-Write for Account (CRITICAL FIX) ---
// This ensures that the entire read/modify/write operation is protected by a single lock.
int atomic_update_account(int userId, int (*modifier)(account_rec_t *acc, void *data), void *modifier_data) {
    int fd = open(ACCOUNTS_DB_FILE, O_RDWR);
    if (fd < 0) return 0;

    lock_file(fd);
    account_rec_t tmp;
    off_t pos = 0;
    int success = 0;

    while (read(fd, &tmp, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
        if (tmp.user_id == userId) {
            // Found the account. Now attempt to modify it.
            if (modifier(&tmp, modifier_data)) {
                // Modification successful, now write back.
                lseek(fd, pos, SEEK_SET); // Seek back to start of record
                if (write(fd, &tmp, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
                    success = 1;
                }
            }
            break; // Stop loop once user is found (modified or not)
        }
        pos += sizeof(account_rec_t);
    }

    unlock_file(fd);
    close(fd);
    return success;
}

// --- Persistence Helpers (Updated to use consistent naming) ---

// Read account
int read_account(int userId, account_rec_t *acc) {
    int fd = open(ACCOUNTS_DB_FILE, O_RDONLY);
    if(fd < 0) return 0;

    lock_file(fd);
    account_rec_t tmp;
    int found = 0;
    while(read(fd, &tmp, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
        if(tmp.user_id == userId) {
            *acc = tmp;
            found = 1;
            break;
        }
    }
    unlock_file(fd);
    close(fd);
    return found;
}

// Write account (update existing)
int write_account(account_rec_t *acc) {
    int fd = open(ACCOUNTS_DB_FILE, O_RDWR | O_CREAT, 0666);
    if(fd < 0) return 0;

    lock_file(fd);
    account_rec_t tmp;
    off_t pos = 0;
    int found = 0;
    while(read(fd, &tmp, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
        if(tmp.user_id == acc->user_id) {
            lseek(fd, pos, SEEK_SET);
            write(fd, acc, sizeof(account_rec_t));
            found = 1;
            break;
        }
        pos += sizeof(account_rec_t);
    }
    
    if (!found) { // Append if not found (for new customer)
        lseek(fd, 0, SEEK_END);
        write(fd, acc, sizeof(account_rec_t));
    }
    
    unlock_file(fd);
    close(fd);
    return 1;
}

// Read user
int read_user(int userId, user_rec_t *user) {
    int fd = open(USERS_DB_FILE, O_RDONLY);
    if(fd < 0) return 0;

    lock_file(fd);
    user_rec_t tmp;
    int found = 0;
    while(read(fd, &tmp, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        if(tmp.user_id == userId) {
            *user = tmp;
            found = 1;
            break;
        }
    }
    unlock_file(fd);
    close(fd);
    return found;
}

// Write user (update existing)
int write_user(user_rec_t *user) {
    int fd = open(USERS_DB_FILE, O_RDWR | O_CREAT, 0666);
    if(fd < 0) return 0;

    lock_file(fd);
    user_rec_t tmp;
    off_t pos = 0;
    int found = 0;
    while(read(fd, &tmp, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        if(tmp.user_id == user->user_id) {
            lseek(fd, pos, SEEK_SET);
            write(fd, user, sizeof(user_rec_t));
            found = 1;
            break;
        }
        pos += sizeof(user_rec_t);
    }
    
    if (!found) { // Append if not found (for new user)
        lseek(fd, 0, SEEK_END);
        write(fd, user, sizeof(user_rec_t));
    }

    unlock_file(fd);
    close(fd);
    return 1;
}

// Append transaction
int append_transaction(txn_rec_t *tx) {
    int fd = open(TRANSACTIONS_DB_FILE, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if(fd < 0) return 0;

    lock_file(fd);
    // Simple ID generation
    tx->txn_id = lseek(fd, 0, SEEK_END) / sizeof(txn_rec_t) + 1;
    write(fd, tx, sizeof(txn_rec_t));
    unlock_file(fd);
    close(fd);
    return 1;
}

// Read loan
int read_loan(uint64_t loanId, loan_rec_t *loan) {
    int fd = open(LOANS_DB_FILE, O_RDONLY);
    if(fd < 0) return 0;

    lock_file(fd);
    loan_rec_t tmp;
    int found = 0;
    while(read(fd, &tmp, sizeof(loan_rec_t)) == sizeof(loan_rec_t)) {
        if(tmp.loan_id == loanId) {
            *loan = tmp;
            found = 1;
            break;
        }
    }
    unlock_file(fd);
    close(fd);
    return found;
}

// Write loan
int write_loan(loan_rec_t *loan) {
    int fd = open(LOANS_DB_FILE, O_RDWR | O_CREAT, 0666);
    if(fd < 0) return 0;

    lock_file(fd);
    loan_rec_t tmp;
    off_t pos = 0;
    int found = 0;
    while(read(fd, &tmp, sizeof(loan_rec_t)) == sizeof(loan_rec_t)) {
        if(tmp.loan_id == loan->loan_id) {
            lseek(fd, pos, SEEK_SET);
            write(fd, loan, sizeof(loan_rec_t));
            found = 1;
            break;
        }
        pos += sizeof(loan_rec_t);
    }
    
    if (!found) { // Should not happen for write_loan, but good practice
        lseek(fd, 0, SEEK_END);
        write(fd, loan, sizeof(loan_rec_t));
    }

    unlock_file(fd);
    close(fd);
    return 1;
}

// Append loan (New function for applying a loan)
int append_loan(loan_rec_t *loan) {
    int fd = open(LOANS_DB_FILE, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if(fd < 0) return 0;

    lock_file(fd);
    // Simple ID generation
    loan->loan_id = lseek(fd, 0, SEEK_END) / sizeof(loan_rec_t) + 1;
    write(fd, loan, sizeof(loan_rec_t));
    
    unlock_file(fd);
    close(fd);
    return 1;
}

// Append feedback
int append_feedback(feedback_rec_t *fb) {
    int fd = open(FEEDBACK_DB_FILE, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if(fd < 0) return 0;

    lock_file(fd);
    // Simple ID generation
    fb->fb_id = lseek(fd, 0, SEEK_END) / sizeof(feedback_rec_t) + 1;
    write(fd, fb, sizeof(feedback_rec_t));
    unlock_file(fd);
    close(fd);
    return 1;
}

// Write feedback
int write_feedback(feedback_rec_t *fb) {
    int fd = open(FEEDBACK_DB_FILE, O_RDWR | O_CREAT, 0666);
    if(fd < 0) return 0;

    lock_file(fd);
    feedback_rec_t tmp;
    off_t pos = 0;
    int found = 0;
    while(read(fd, &tmp, sizeof(feedback_rec_t)) == sizeof(feedback_rec_t)) {
        if(tmp.fb_id == fb->fb_id) {
            lseek(fd, pos, SEEK_SET);
            write(fd, fb, sizeof(feedback_rec_t));
            found = 1;
            break;
        }
        pos += sizeof(feedback_rec_t);
    }
    
    if (!found) {
        lseek(fd, 0, SEEK_END);
        write(fd, fb, sizeof(feedback_rec_t));
    }

    unlock_file(fd);
    close(fd);
    return 1;
}

// Read feedback (by ID)
int read_feedback(uint64_t fbId, feedback_rec_t *fb) {
    int fd = open(FEEDBACK_DB_FILE, O_RDONLY);
    if(fd < 0) return 0;

    lock_file(fd);
    feedback_rec_t tmp;
    int found = 0;
    while(read(fd, &tmp, sizeof(feedback_rec_t)) == sizeof(feedback_rec_t)) {
        if(tmp.fb_id == fbId) {
            *fb = tmp;
            found = 1;
            break;
        }
    }
    unlock_file(fd);
    close(fd);
    return found;
}

// Simple unique ID generation
int generate_new_userId() {
    int fd = open(USERS_DB_FILE, O_RDONLY | O_CREAT, 0666);
    if (fd < 0) return 1001; // Start at 1001 if file doesn't exist
    
    lock_file(fd);
    // We assume the caller handles the mutex for the USERS_DB_FILE if needed.
    // Given the write_user logic, we can rely on file size for a simple unique ID.
    int count = lseek(fd, 0, SEEK_END) / sizeof(user_rec_t);
    unlock_file(fd);
    close(fd);
    return 1001 + count; // Start IDs from 1001
}