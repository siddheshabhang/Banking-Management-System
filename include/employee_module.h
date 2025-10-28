#ifndef EMPLOYEE_MODULE_H
#define EMPLOYEE_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include "utils.h" // For user_rec_t and account_rec_t

int add_new_customer(user_rec_t *user, account_rec_t *acc, const char *username, const char *password, char *resp_msg, size_t resp_sz);
int modify_customer(uint32_t user_id, const char *name, int age, const char *address, char *resp_msg, size_t resp_sz);
int approve_reject_loan(uint64_t loan_id, const char *action, uint32_t emp_id, char *resp_msg, size_t resp_sz);
int view_assigned_loans(uint32_t emp_id, char *resp_msg, size_t resp_sz);
int process_loans(char *resp_msg, size_t resp_sz);
int view_customer_transactions(uint32_t custId, char *resp_msg, size_t resp_sz);

#endif