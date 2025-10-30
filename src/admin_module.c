#include "admin_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

static const char* get_role_str_for_list(role_t role) {
    switch(role) {
        case ROLE_CUSTOMER: return "CUSTOMER";
        case ROLE_EMPLOYEE: return "EMPLOYEE";
        case ROLE_MANAGER:  return "MANAGER";
        case ROLE_ADMIN:    return "ADMIN";
        default:            return "UNKNOWN";
    }
}

int add_employee(const char *name, int age, const char *address, const char *role_str,
                 const char *username, const char *password, char *resp_msg, size_t resp_sz) {
    
    user_rec_t user;
    memset(&user, 0, sizeof(user_rec_t));

    user.user_id = generate_new_userId();
    user.age = age;
    user.active = STATUS_ACTIVE;
    user.created_at = time(NULL);
    
    strncpy(user.name, name, sizeof(user.name) - 1);
    strncpy(user.address, address, sizeof(user.address) - 1);
    strncpy(user.username, username, sizeof(user.username) - 1);

    // Convert role string to enum
    if (strcmp(role_str, "employee") == 0) user.role = ROLE_EMPLOYEE;
    else if (strcmp(role_str, "manager") == 0) user.role = ROLE_MANAGER;
    else if (strcmp(role_str, "admin") == 0) user.role = ROLE_ADMIN;
    else {
        snprintf(resp_msg, resp_sz, "Invalid role. Must be 'employee', 'manager', or 'admin'.");
        return 0;
    }
    
    // CRITICAL SECURITY FIX: Hash the password
    generate_password_hash(password, user.password_hash, sizeof(user.password_hash));

    if(write_user(&user)) {
        snprintf(resp_msg,resp_sz,"Employee Added (ID: %u, Role: %s)", user.user_id, role_str);
        return 1;
    }
    
    snprintf(resp_msg,resp_sz,"Employee Add Failed (Write Error)");
    return 0;
}

int modify_user(uint32_t userId, const char *name, int age, const char *address, char *resp_msg, size_t resp_sz) {
    user_rec_t user;
    if(!read_user(userId,&user)) { 
        snprintf(resp_msg,resp_sz,"User not found"); 
        return 0; 
    }
    
    strncpy(user.name, name, sizeof(user.name) - 1);
    user.age = age;
    strncpy(user.address, address, sizeof(user.address) - 1);
    
    if(write_user(&user)) {
        snprintf(resp_msg, resp_sz, 
                 "User Modified (ID: %u).\nNew Details:\nName: %s\nAge: %d\nAddress: %s", 
                 userId, user.name, user.age, user.address);
        return 1;
    }
    
    snprintf(resp_msg,resp_sz,"User Modification Failed");
    return 0;
}

int change_user_role(uint32_t userId, const char *role_str, char *resp_msg, size_t resp_sz) {
    user_rec_t user;
    if(!read_user(userId,&user)) { 
        snprintf(resp_msg,resp_sz,"User not found"); 
        return 0; 
    }
    
    role_t new_role;
    // Convert role string to enum
    if (strcmp(role_str, "customer") == 0) new_role = ROLE_CUSTOMER;
    else if (strcmp(role_str, "employee") == 0) new_role = ROLE_EMPLOYEE;
    else if (strcmp(role_str, "manager") == 0) new_role = ROLE_MANAGER;
    else if (strcmp(role_str, "admin") == 0) new_role = ROLE_ADMIN;
    else {
        snprintf(resp_msg, resp_sz, "Invalid role. Must be 'customer', 'employee', 'manager', or 'admin'.");
        return 0;
    }
    
    user.role = new_role;
    
    if(write_user(&user)) {
        snprintf(resp_msg,resp_sz,"User %u Role Updated to %s", userId, role_str);
        return 1;
    }
    
    snprintf(resp_msg,resp_sz,"Role Update Failed");
    return 0;
}

int list_all_users(char *resp_msg, size_t resp_sz) {
    int fd = open(USERS_DB_FILE, O_RDONLY);
    if(fd < 0) { 
        snprintf(resp_msg, resp_sz, "Failed to open user database"); 
        return 0; 
    }
    
    lock_file(fd); // Use read-lock
    
    user_rec_t user;
    char tmp[256]; 
    resp_msg[0] = '\0'; // Start with an empty string
    int found = 0;
    size_t current_len = 0;

    // Add a header to the response
    snprintf(resp_msg, resp_sz, "--- User List ---\n");
    strncat(resp_msg, "ID   | Username        | Role\n", resp_sz - strlen(resp_msg) - 1);
    strncat(resp_msg, "---- | --------------- | --------\n", resp_sz - strlen(resp_msg) - 1);
    current_len = strlen(resp_msg);
    
    while(read(fd, &user, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        // List everyone *except* other Admins
        if(user.role != ROLE_ADMIN) {
            snprintf(tmp, sizeof(tmp), "%-4u | %-15s | %s\n",
                     user.user_id, 
                     user.username, 
                     get_role_str_for_list(user.role));
            
            // Check if we have space to add this new line
            if (current_len + strlen(tmp) < resp_sz) {
                strncat(resp_msg, tmp, resp_sz - current_len - 1);
                current_len += strlen(tmp);
            } else {
                // Buffer is full, stop adding users
                strncat(resp_msg, "... (list truncated) ...\n", resp_sz - current_len - 1);
                break; 
            }
            found = 1;
        }
    }
    
    unlock_file(fd);
    close(fd);
    
    if (!found) {
        snprintf(resp_msg, resp_sz, "No customers or employees found.");
    }
    return 1;
}