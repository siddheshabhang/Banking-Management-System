#ifndef ADMIN_MODULE_H
#define ADMIN_MODULE_H

#include <stdint.h>
#include <stddef.h>
#include "utils.h"

int add_employee(const char *name, int age, const char *address, const char *role, const char *username, const char *password, char *resp_msg, size_t resp_sz);
int modify_user(uint32_t userId, const char *name, int age, const char *address, char *resp_msg, size_t resp_sz);
int change_user_role(uint32_t userId, const char *role, char *resp_msg, size_t resp_sz);

#endif