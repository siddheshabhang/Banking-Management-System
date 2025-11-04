#ifndef ADMIN_MODULE_H
#define ADMIN_MODULE_H

#include <stdint.h>
#include <stddef.h>
#include "utils.h"

/* --- ADMIN MODULE INTERFACE (Staff/Role Mgmt) --- */
int add_employee(const char *first_name, const char *last_name, int age, const char *address, const char *role, const char *email, const char *phone, const char *username, const char *password, char *resp_msg, size_t resp_sz);
int modify_user(uint32_t userId, const char *first_name, const char *last_name, int age, const char *address, const char *email, const char *phone, char *resp_msg, size_t resp_sz);
int change_user_role(uint32_t userId, const char *role, char *resp_msg, size_t resp_sz);
int list_all_users(char *resp_msg, size_t resp_sz);

#endif