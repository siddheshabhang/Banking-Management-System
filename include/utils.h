#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Include server.h to access the globally defined structs
#include "server.h"

// --- File Locking ---
int lock_file(int fd);
int unlock_file(int fd);

// --- User Persistence (using user_rec_t) ---
int write_user(user_rec_t *user);
int read_user(int userId, user_rec_t *user);
int generate_new_userId();

// --- Account Persistence (using account_rec_t) ---
int write_account(account_rec_t *acc);
int read_account(int userId, account_rec_t *acc);

// --- Transaction Persistence (using txn_rec_t) ---
int append_transaction(txn_rec_t *tx);

// --- Loan Persistence (using loan_rec_t) ---
int read_loan(uint64_t loanId, loan_rec_t *loan);
int write_loan(loan_rec_t *loan);
int append_loan(loan_rec_t *loan); // Added for new loan applications

// --- Feedback Persistence (using feedback_rec_t) ---
int append_feedback(feedback_rec_t *fb);
int write_feedback(feedback_rec_t *fb);
int read_feedback(uint64_t fbId, feedback_rec_t *fb);

// --- Security & Auth (CRITICAL FIX: Hashing) ---
int login_user(const char *username, const char *password, int *userId, char *role, size_t role_sz);
void generate_password_hash(const char *password, char *hash_output, size_t hash_size);
int verify_password(const char *password, const char *hash);

// --- Concurrency FIX: Atomic Read-Modify-Write for Account ---
// This function holds the lock across the entire read/write cycle for safety.
int atomic_update_account(int userId, int (*modifier)(account_rec_t *acc, void *data), void *modifier_data);

#endif