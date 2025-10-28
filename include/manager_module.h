#ifndef MANAGER_MODULE_H
#define MANAGER_MODULE_H

#include <stddef.h>
#include <stdint.h>
#include "utils.h"

int set_account_status(uint32_t custId, int status, char *resp_msg, size_t resp_sz);
int view_non_assigned_loans(char *resp_msg, size_t resp_sz);
int assign_loan_to_employee(uint32_t loanId, uint32_t empId, char *resp_msg, size_t resp_sz);
int review_feedbacks(char *resp_msg, size_t resp_sz);

#endif