#include "utils.h"
#include <sys/file.h>
#include <unistd.h> // For crypt() on macOS

/*
 * ===================================================================
 * HELPER PROTOTYPES
 * (These are internal to utils.c and don't need to be in utils.h)
 * ===================================================================
 */

// Generic record locking
static int lock_record(int fd, long offset, size_t record_size);
static int unlock_record(int fd, long offset, size_t record_size);

// Finders
static long find_user_offset(uint32_t userId);
static long find_account_offset(uint32_t userId);
static long find_loan_offset(uint64_t loanId);

/*
 * ===================================================================
 * GENERIC LOCKING FUNCTIONS
 * ===================================================================
 */

// Acquire exclusive lock on ENTIRE file
int lock_file(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0; // 0 means to lock to EOF
    return fcntl(fd, F_SETLKW, &lock);
}

// Unlock ENTIRE file
int unlock_file(int fd) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    return fcntl(fd, F_SETLKW, &lock);
}

// --- Lock a single record for exclusive access (Write Lock) ---
static int lock_record(int fd, long offset, size_t record_size) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK; // Exclusive Write Lock
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = record_size; // Lock ONLY the size of one record
    return fcntl(fd, F_SETLKW, &lock); 
}

// --- Unlock a single record ---
static int unlock_record(int fd, long offset, size_t record_size) {
    struct flock lock;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_UNLCK; 
    lock.l_whence = SEEK_SET;
    lock.l_start = offset;
    lock.l_len = record_size; 
    return fcntl(fd, F_SETLKW, &lock);
}

/*
 * ===================================================================
 * AUTH & HASHING
 * ===================================================================
 */

// Generate hash of password
void generate_password_hash(const char *password, char *hash_output, size_t hash_size) {
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

// --- User Login Function with Account Status Check (CRITICAL FIX) ---
// Updated to return the user's name
int login_user(const char *username, const char *password, int *userId, char *role, size_t role_sz, char *name_out, size_t name_sz) {
    int fd = open(USERS_DB_FILE, O_RDONLY);
    if (fd < 0) return 0;
    
    lock_file(fd);
    user_rec_t user;
    int found = 0; 
    
    // Clear the name buffer
    name_out[0] = '\0';

    while (read(fd, &user, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        if (strcmp(user.username, username) == 0) {
            
            if (verify_password(password, user.password_hash)) {
                
                account_rec_t acc;
                int account_found = read_account(user.user_id, &acc);

                if (user.active == STATUS_INACTIVE || (account_found && acc.active == STATUS_INACTIVE)) {
                    found = 2; // Inactive
                } else {
                    *userId = user.user_id;
                    
                    // Copy the name out
                    strncpy(name_out, user.name, name_sz - 1);
                    
                    // Map role enum to string
                    if (user.role == ROLE_CUSTOMER) strncpy(role, "customer", role_sz);
                    else if (user.role == ROLE_EMPLOYEE) strncpy(role, "employee", role_sz);
                    else if (user.role == ROLE_MANAGER) strncpy(role, "manager", role_sz);
                    else if (user.role == ROLE_ADMIN) strncpy(role, "admin", role_sz);
                    else strncpy(role, "unknown", role_sz);

                    found = 1; // SUCCESS
                }
            }
            break; 
        }
    }
    unlock_file(fd);
    close(fd);
    return found;
}

/*
 * ===================================================================
 * NEW ATOMIC R-M-W HANDLERS
 * ===================================================================
 */
// --- Offset Finder for Users ---
static long find_user_offset(uint32_t userId) {
    int fd = open(USERS_DB_FILE, O_RDONLY);
    if (fd < 0) return -1;
    lock_file(fd); // Full file lock to safely search
    user_rec_t tmp;
    long current_offset = 0;
    long found_offset = -1;
    while (read(fd, &tmp, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        if (tmp.user_id == userId) {
            found_offset = current_offset;
            break; 
        }
        current_offset += sizeof(user_rec_t); 
    }
    unlock_file(fd);
    close(fd);
    return found_offset;
}

// --- Atomic R-M-W for users.db ---
int atomic_update_user(uint32_t userId, int (*modifier)(user_rec_t *user, void *data), void *modifier_data) {
    long offset = find_user_offset(userId);
    if (offset < 0) return 0; // Not found
    
    int fd = open(USERS_DB_FILE, O_RDWR);
    if (fd < 0) return 0;

    if (lock_record(fd, offset, sizeof(user_rec_t)) != 0) {
        close(fd); return 0; // Lock failed
    }

    int success = 0;
    user_rec_t tmp;
    
    lseek(fd, offset, SEEK_SET); 
    if (read(fd, &tmp, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        if (modifier(&tmp, modifier_data)) {
            lseek(fd, offset, SEEK_SET); 
            if (write(fd, &tmp, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
                success = 1;
            }
        }
    }

    unlock_record(fd, offset, sizeof(user_rec_t));
    close(fd);
    return success;
}

// --- Offset Finder for Accounts ---
static long find_account_offset(uint32_t userId) {
    int fd = open(ACCOUNTS_DB_FILE, O_RDONLY);
    if (fd < 0) return -1;
    lock_file(fd); // Full file lock to safely search
    account_rec_t tmp;
    long current_offset = 0;
    long found_offset = -1;
    while (read(fd, &tmp, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
        if (tmp.user_id == userId) {
            found_offset = current_offset;
            break; 
        }
        current_offset += sizeof(account_rec_t); 
    }
    unlock_file(fd);
    close(fd);
    return found_offset;
}

// --- Atomic R-M-W for accounts.db ---
int atomic_update_account(uint32_t userId, int (*modifier)(account_rec_t *acc, void *data), void *modifier_data) {
    long offset = find_account_offset(userId);
    if (offset < 0) return 0; // Not found
    
    int fd = open(ACCOUNTS_DB_FILE, O_RDWR);
    if (fd < 0) return 0;

    if (lock_record(fd, offset, sizeof(account_rec_t)) != 0) {
        close(fd); return 0; // Lock failed
    }

    int success = 0;
    account_rec_t tmp;
    
    lseek(fd, offset, SEEK_SET); 
    if (read(fd, &tmp, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
        if (modifier(&tmp, modifier_data)) {
            lseek(fd, offset, SEEK_SET); 
            if (write(fd, &tmp, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
                success = 1;
            }
        }
    }

    unlock_record(fd, offset, sizeof(account_rec_t));
    close(fd);
    return success;
}

// --- Offset Finder for Loans ---
static long find_loan_offset(uint64_t loanId) {
    int fd = open(LOANS_DB_FILE, O_RDONLY);
    if (fd < 0) return -1;
    lock_file(fd); // Full file lock to safely search
    loan_rec_t tmp;
    long current_offset = 0;
    long found_offset = -1;
    while (read(fd, &tmp, sizeof(loan_rec_t)) == sizeof(loan_rec_t)) {
        if (tmp.loan_id == loanId) {
            found_offset = current_offset;
            break; 
        }
        current_offset += sizeof(loan_rec_t); 
    }
    unlock_file(fd);
    close(fd);
    return found_offset;
}

// --- Atomic R-M-W for loans.db ---
int atomic_update_loan(uint64_t loanId, int (*modifier)(loan_rec_t *loan, void *data), void *modifier_data) {
    long offset = find_loan_offset(loanId);
    if (offset < 0) return 0; // Not found
    
    int fd = open(LOANS_DB_FILE, O_RDWR);
    if (fd < 0) return 0;

    if (lock_record(fd, offset, sizeof(loan_rec_t)) != 0) {
        close(fd); return 0; // Lock failed
    }

    int success = 0;
    loan_rec_t tmp;
    
    lseek(fd, offset, SEEK_SET); 
    if (read(fd, &tmp, sizeof(loan_rec_t)) == sizeof(loan_rec_t)) {
        if (modifier(&tmp, modifier_data)) {
            lseek(fd, offset, SEEK_SET); 
            if (write(fd, &tmp, sizeof(loan_rec_t)) == sizeof(loan_rec_t)) {
                success = 1;
            }
        }
    }

    unlock_record(fd, offset, sizeof(loan_rec_t));
    close(fd);
    return success;
}


/*
 * ===================================================================
 * NON-ATOMIC PERSISTENCE HELPERS
 * (Used for login, bootstrap, appends, and non-racy operations)
 * ===================================================================
 */

// Read account
int read_account(int userId, account_rec_t *acc) {
    int fd = open(ACCOUNTS_DB_FILE, O_RDONLY);
    if(fd < 0) return 0;
    lock_file(fd); // Full file lock
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

// Write account (update existing or append)
int write_account(account_rec_t *acc) {
    int fd = open(ACCOUNTS_DB_FILE, O_RDWR | O_CREAT, 0666);
    if(fd < 0) return 0;
    lock_file(fd); // Full file lock
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
    if (!found) { 
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
    lock_file(fd); // Full file lock
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

// Write user (update existing or append)
int write_user(user_rec_t *user) {
    int fd = open(USERS_DB_FILE, O_RDWR | O_CREAT, 0666);
    if(fd < 0) return 0;
    lock_file(fd); // Full file lock
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
    if (!found) { 
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
    lock_file(fd); // Full file lock
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

// Write loan (update existing or append)
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
    if (!found) {
        lseek(fd, 0, SEEK_END);
        write(fd, loan, sizeof(loan_rec_t));
    }
    unlock_file(fd);
    close(fd);
    return 1;
}

// Append loan
int append_loan(loan_rec_t *loan) {
    int fd = open(LOANS_DB_FILE, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if(fd < 0) return 0;
    lock_file(fd);
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
    if (fd < 0) return 1001; 
    lock_file(fd);
    int count = lseek(fd, 0, SEEK_END) / sizeof(user_rec_t);
    unlock_file(fd);
    close(fd);
    return 1001 + count; // Start IDs from 1001
}