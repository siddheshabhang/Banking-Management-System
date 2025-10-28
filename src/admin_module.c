#include "admin_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

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
        snprintf(resp_msg,resp_sz,"User Modified");
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