#ifndef BANKING_H
#define BANKING_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h> // For size_t
#include <stdbool.h>
#include <time.h>
#include "server.h" // For data structures

/* ------------------------
   Customer Module
------------------------ */
int view_balance(uint32_t user_id, char *resp_msg, size_t resp_sz);
int deposit_money(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz);
int withdraw_money(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz);
int transfer_funds(uint32_t from_user, uint32_t to_user, double amount, char *resp_msg, size_t resp_sz);
int apply_loan(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz);
int view_transaction_history(uint32_t user_id, char *resp_msg, size_t resp_sz);

/* ------------------------
   Employee Module
------------------------ */
int add_new_customer(user_rec_t *user, account_rec_t *acc, const char *username, const char *password, char *resp_msg, size_t resp_sz);
int modify_customer(uint32_t user_id, const char *name, int age, const char *address, char *resp_msg, size_t resp_sz);
int approve_reject_loan(uint64_t loan_id, const char *action, uint32_t emp_id, char *resp_msg, size_t resp_sz);

#endif