#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "server.h"

/* --- FILE LOCKING (Concurrency: fcntl) --- */
int lock_file(int fd);      // Full-file lock (search/append)
int unlock_file(int fd);

/* --- USER PERSISTENCE --- */
int write_user(user_rec_t *user);
int read_user(int userId, user_rec_t *user);
int generate_new_userId();

/* --- ATOMIC R-M-W HANDLER (RECORD-LEVEL LOCKING) --- */
int atomic_update_user(uint32_t userId, int (*modifier)(user_rec_t *user, void *data), void *modifier_data);

/* --- ACCOUNT PERSISTENCE --- */
int write_account(account_rec_t *acc);
int read_account(int userId, account_rec_t *acc);
int atomic_update_account(uint32_t userId, int (*modifier)(account_rec_t *acc, void *data), void *modifier_data);

/* --- TRANSACTION PERSISTENCE --- */
int append_transaction(txn_rec_t *tx);

/* --- LOAN PERSISTENCE --- */
int read_loan(uint64_t loanId, loan_rec_t *loan);
int write_loan(loan_rec_t *loan);
int append_loan(loan_rec_t *loan); // Added for new loan applications
int atomic_update_loan(uint64_t loanId, int (*modifier)(loan_rec_t *loan, void *data), void *modifier_data);

/* --- FEEDBACK PERSISTENCE --- */
int append_feedback(feedback_rec_t *fb);
int write_feedback(feedback_rec_t *fb);
int read_feedback(uint64_t fbId, feedback_rec_t *fb);

/* --- SECURITY & AUTH --- */
int login_user(const char *username, const char *password, int *userId, char *role, size_t role_sz, char *fname_out, size_t fname_sz);
void generate_password_hash(const char *password, char *hash_output, size_t hash_size);
int verify_password(const char *password, const char *hash);
int check_uniqueness(const char* username, const char* email, const char* phone, uint32_t current_user_id, char* resp_msg, size_t resp_sz);

#endif