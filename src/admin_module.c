#include "admin_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

static const char *get_role_str_for_list(role_t role)
{
    switch (role)
    {
    case ROLE_CUSTOMER:
        return "CUSTOMER";
    case ROLE_EMPLOYEE:
        return "EMPLOYEE";
    case ROLE_MANAGER:
        return "MANAGER";
    case ROLE_ADMIN:
        return "ADMIN";
    default:
        return "UNKNOWN";
    }
}

// --- Modifier for modify_user ---
typedef struct
{
    const char *first_name;
    const char *last_name;
    int age;
    const char *address;
    const char *email;
    const char *phone;
    char *resp_msg;
    size_t resp_sz;
} modify_user_data;

int modify_user_modifier(user_rec_t *user, void *data)
{
    modify_user_data *d = (modify_user_data *)data;
    if (!check_uniqueness(user->username, d->email, d->phone, user->user_id, d->resp_msg, d->resp_sz))
    {
        return 0; // Abort, resp_msg already set by check_uniqueness
    }
    strncpy(user->first_name, d->first_name, sizeof(user->first_name) - 1);
    strncpy(user->last_name, d->last_name, sizeof(user->last_name) - 1);
    user->age = d->age;
    strncpy(user->address, d->address, sizeof(user->address) - 1);
    strncpy(user->email, d->email, sizeof(user->email) - 1);
    strncpy(user->phone, d->phone, sizeof(user->phone) - 1);

    snprintf(d->resp_msg, d->resp_sz, "User Modified (ID: %u).\nNew Details:\nName: %s %s\nAge: %d\nAddress: %s\nEmail: %s\nPhone: %s", user->user_id, user->first_name, user->last_name, user->age, user->address, user->email, user->phone);
    return 1; // Commit
}

// --- REFACTORED modify_user ---
int modify_user(uint32_t userId, const char *first_name, const char *last_name, int age, const char *address, const char *email, const char *phone, char *resp_msg, size_t resp_sz)
{
    modify_user_data data = {first_name, last_name, age, address, email, phone, resp_msg, resp_sz};

    if (atomic_update_user(userId, modify_user_modifier, &data))
    {
        return 1;
    }

    snprintf(resp_msg, resp_sz, "User Modification Failed (User not found or concurrency error)");
    return 0;
}

// --- Modifier for change_user_role ---
typedef struct
{
    const char *role_str;
    char *resp_msg;
    size_t resp_sz;
} change_role_data;

int change_role_modifier(user_rec_t *user, void *data)
{
    change_role_data *d = (change_role_data *)data;

    role_t new_role;
    if (strcmp(d->role_str, "customer") == 0)
        new_role = ROLE_CUSTOMER;
    else if (strcmp(d->role_str, "employee") == 0)
        new_role = ROLE_EMPLOYEE;
    else if (strcmp(d->role_str, "manager") == 0)
        new_role = ROLE_MANAGER;
    else if (strcmp(d->role_str, "admin") == 0)
        new_role = ROLE_ADMIN;
    else
    {
        snprintf(d->resp_msg, d->resp_sz, "Invalid role. Must be 'customer', 'employee', 'manager', or 'admin'.");
        return 0; // Abort
    }

    user->role = new_role;
    snprintf(d->resp_msg, d->resp_sz, "User %u Role Updated to %s", user->user_id, d->role_str);
    return 1; // Commit
}

// --- REFACTORED change_user_role ---
int change_user_role(uint32_t userId, const char *role_str, char *resp_msg, size_t resp_sz)
{
    change_role_data data = {role_str, resp_msg, resp_sz};

    if (atomic_update_user(userId, change_role_modifier, &data))
    {
        return 1;
    }

    if (resp_msg[0] == '\0')
    {
        snprintf(resp_msg, resp_sz, "Role Update Failed (User not found or concurrency error)");
    }
    return 0;
}

// --- (Other functions remain the same) ---

// This function is safe (append-only)
int add_employee(const char *first_name, const char *last_name, int age, const char *address, const char *role_str, const char *email, const char *phone, const char *username, const char *password, char *resp_msg, size_t resp_sz)
{
    if (!check_uniqueness(username, email, phone, 0, resp_msg, resp_sz))
    {             // [cite: 2]
        return 0; // Fail, resp_msg is set by check_uniqueness
    }
    user_rec_t user;
    memset(&user, 0, sizeof(user_rec_t));

    user.user_id = generate_new_userId();
    user.age = age;
    user.active = STATUS_ACTIVE;
    user.created_at = time(NULL);

    strncpy(user.first_name, first_name, sizeof(user.first_name) - 1);
    strncpy(user.last_name, last_name, sizeof(user.last_name) - 1);
    strncpy(user.address, address, sizeof(user.address) - 1);
    strncpy(user.username, username, sizeof(user.username) - 1);
    strncpy(user.email, email, sizeof(user.email) - 1);
    strncpy(user.phone, phone, sizeof(user.phone) - 1);

    if (strcmp(role_str, "employee") == 0)
        user.role = ROLE_EMPLOYEE;
    else if (strcmp(role_str, "manager") == 0)
        user.role = ROLE_MANAGER;
    else if (strcmp(role_str, "admin") == 0)
        user.role = ROLE_ADMIN;
    else
    {
        snprintf(resp_msg, resp_sz, "Invalid role. Must be 'employee', 'manager', or 'admin'.");
        return 0;
    }

    generate_password_hash(password, user.password_hash, sizeof(user.password_hash));

    if (write_user(&user))
    {
        snprintf(resp_msg, resp_sz, "Employee Added (ID: %u, Role: %s)", user.user_id, role_str);
        return 1;
    }

    snprintf(resp_msg, resp_sz, "Employee Add Failed (Write Error)");
    return 0;
}

// This function is safe (read-only list)
int list_all_users(char *resp_msg, size_t resp_sz)
{
    int fd = open(USERS_DB_FILE, O_RDONLY);
    if (fd < 0)
    {
        snprintf(resp_msg, resp_sz, "Failed to open user database");
        return 0;
    }

    lock_file(fd);

    user_rec_t user;
    char tmp[256];
    resp_msg[0] = '\0';
    int found = 0;
    size_t current_len = 0;

    snprintf(resp_msg, resp_sz, "--- User List ---\n");
    strncat(resp_msg, "ID   | Username        | Role\n", resp_sz - strlen(resp_msg) - 1);
    strncat(resp_msg, "---- | --------------- | --------\n", resp_sz - strlen(resp_msg) - 1);
    current_len = strlen(resp_msg);

    while (read(fd, &user, sizeof(user_rec_t)) == sizeof(user_rec_t))
    {
        if (user.role != ROLE_ADMIN)
        {
            snprintf(tmp, sizeof(tmp), "%-4u | %-15s | %s\n",
                     user.user_id,
                     user.username,
                     get_role_str_for_list(user.role));

            if (current_len + strlen(tmp) < resp_sz)
            {
                strncat(resp_msg, tmp, resp_sz - current_len - 1);
                current_len += strlen(tmp);
            }
            else
            {
                strncat(resp_msg, "... (list truncated) ...\n", resp_sz - current_len - 1);
                break;
            }
            found = 1;
        }
    }

    unlock_file(fd);
    close(fd);

    if (!found)
    {
        snprintf(resp_msg, resp_sz, "No customers or employees found.");
    }
    return 1;
}