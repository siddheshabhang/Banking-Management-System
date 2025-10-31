/*
 * Save this file as: src/bootstrap.c
 */
#include "admin_module.h" // Needed for Admin/Staff creation
#include "employee_module.h" // Needed for Customer creation (since employee handles it)
#include "utils.h"        
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>     
#include <errno.h>

/*
 * This program runs ONCE to create the first admin user, plus one of each other role.
 */
int main() {
    printf("Bootstrapping system with one user for each staff role, and two customers...\n");
    
    // Ensure the db directory exists
    // DB_DIR is defined in server.h, which is included via utils.h
    if (mkdir(DB_DIR, 0700) == -1 && errno != EEXIST) {
        perror("Failed to create db directory");
        return 1;
    } else {
        printf("db directory ensured.\n");
    }
    
    char resp_msg[MAX_MSG_LEN];
    int success_count = 0;

    // --- 1. ADMIN USER (Siddhesh Abhang, ID: 1001) ---
    printf("\n--- Creating ADMIN (Siddhesh Abhang) ---\n");
    if (add_employee("Siddhesh Abhang", 35, "Pune HQ", "admin", "siddhesh", "adminpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- 2. MANAGER USER (Manasi Joshi, ID: 1002) ---
    printf("\n--- Creating MANAGER (Manasi Joshi) ---\n");
    if (add_employee("Manasi Joshi", 45, "Mumbai Office", "manager", "manasi", "managerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }
    
    // --- 3. EMPLOYEE USER (Eknath Shinde, ID: 1003) ---
    printf("\n--- Creating EMPLOYEE (Eknath Shinde) ---\n");
    if (add_employee("Eknath Shinde", 28, "Teller Desk", "employee", "eknath", "employeepass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }
    
    // --- 4. CUSTOMER 1 (Chandrika Patil, ID: 1004) ---
    printf("\n--- Creating CUSTOMER 1 (Chandrika Patil) ---\n");
    user_rec_t cust1_user = {0}; 
    account_rec_t cust1_acc = {0}; 
    
    strncpy(cust1_user.name, "Chandrika Patil", sizeof(cust1_user.name) - 1);
    cust1_user.age = 30;
    strncpy(cust1_user.address, "1 Main Street, Nagpur", sizeof(cust1_user.address) - 1);
    
    if (add_new_customer(&cust1_user, &cust1_acc, "chandrika", "customerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- 5. CUSTOMER 2 (Rohan Deshmukh, ID: 1005) ---
    printf("\n--- Creating CUSTOMER 2 (Rohan Deshmukh) ---\n");
    user_rec_t cust2_user = {0}; 
    account_rec_t cust2_acc = {0}; 
    
    strncpy(cust2_user.name, "Rohan Deshmukh", sizeof(cust2_user.name) - 1);
    cust2_user.age = 25;
    strncpy(cust2_user.address, "2 Bank Road, Pune", sizeof(cust2_user.address) - 1);
    
    if (add_new_customer(&cust2_user, &cust2_acc, "rohan", "customerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- 6. CUSTOMER 3 (Nandini Patil, ID: 1006) ---
    printf("\n--- Creating CUSTOMER 3 (Nandini Patil) ---\n");
    user_rec_t cust3_user = {0}; 
    account_rec_t cust3_acc = {0}; 

    strncpy(cust3_user.name, "Nandini Patil", sizeof(cust3_user.name) - 1);
    cust3_user.age = 27;
    strncpy(cust3_user.address, "Sinhagad Road, Pune", sizeof(cust3_user.address) - 1);

    if (add_new_customer(&cust3_user, &cust3_acc, "nandini", "customerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- 7. CUSTOMER 4 (Vamsi Krishna, ID: 1007) ---
    printf("\n--- Creating CUSTOMER 4 (Vamsi Krishna) ---\n");
    user_rec_t cust4_user = {0}; 
    account_rec_t cust4_acc = {0}; 

    strncpy(cust4_user.name, "Vamsi Krishna", sizeof(cust4_user.name) - 1);
    cust4_user.age = 32;
    strncpy(cust4_user.address, "Vishakapatnam", sizeof(cust4_user.address) - 1);

    if (add_new_customer(&cust4_user, &cust4_acc, "vamsi", "customerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    printf("\n--- Bootstrapping Complete ---\n");
    printf("%d out of 7 initial users successfully created.\n", success_count);
    
    if (success_count == 5) {
        printf("\nDefault Credentials for Testing:\n");
        printf("Role     | Name (ID)             | Username  | Password\n");
        printf("---------|-----------------------|-----------|-------------\n");
        printf("Admin    | Siddhesh Abhang (1001)| siddhesh  | adminpass\n");
        printf("Manager  | Manasi Joshi (1002)   | manasi    | managerpass\n");
        printf("Employee | Eknath Shinde (1003)  | eknath    | employeepass\n");
        printf("Customer1| Chandrika Patil (1004)| chandrika | customerpass\n");
        printf("Customer2| Rohan Deshmukh (1005) | rohan     | customerpass\n");
        printf("Customer3| Nandini Patil (1006)  | nandini   | customerpass\n");
        printf("Customer4| Vamsi Krishna (1007)  | vamsi     | customerpass\n");
    }
    
    return 0;
}