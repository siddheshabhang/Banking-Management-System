#ifndef CUSTOMER_MODULE_H
#define CUSTOMER_MODULE_H

#include <stdint.h>
#include <stddef.h>
#include "server.h" 

/* --- CUSTOMER MODULE INTERFACE (Atomic financial operations) --- */
int view_balance(uint32_t user_id, char *resp_msg, size_t resp_sz);
int deposit_money(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz);
int withdraw_money(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz);
int transfer_funds(uint32_t from_id, uint32_t to_id, double amount, char *resp_msg, size_t resp_sz);
int apply_loan(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz);
int view_loan_status(uint32_t user_id, char *resp_msg, size_t resp_sz);
int add_feedback(uint32_t user_id, const char *msg, char *resp_msg, size_t resp_sz);
int view_feedback_status(uint32_t user_id, char *resp_msg, size_t resp_sz);
int view_transaction_history(uint32_t user_id, char *resp_msg, size_t resp_sz);
int change_password(uint32_t user_id, const char *newpass, char *resp_msg, size_t resp_sz);
int view_personal_details(uint32_t user_id, char *resp_msg, size_t resp_sz);

#endif