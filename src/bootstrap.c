#include "admin_module.h" // Needed for Admin/Staff creation
#include "employee_module.h" // Needed for Customer creation (since employee handles it)
#include "utils.h"        
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>     
#include <errno.h>

/*
 * This program runs ONCE to create the first admin user, plus one of each other role.
 * NOTE: Address fields cannot contain spaces. Use underscores.
 */
int main() {
    printf("Bootstrapping system with initial staff and customers...\n");
    
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
    if (add_employee("Siddhesh", "Abhang", 35, "Pune_HQ", "admin", "siddhesh@bank.com", "9876543210", 
                     "siddhesh", "adminpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- 2. MANAGER USER (Manasi Joshi, ID: 1002) ---
    printf("\n--- Creating MANAGER (Manasi Joshi) ---\n");
    if (add_employee("Manasi", "Joshi", 45, "Mumbai_Office", "manager", "manasi@bank.com", "9876543211",
                     "manasi", "managerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }
    
    // --- 3. EMPLOYEE USER (Prakash Kadam, ID: 1003) ---
    printf("\n--- Creating EMPLOYEE (Prakash Kadam) ---\n");
    if (add_employee("Prakash", "Kadam", 28, "Teller_Desk", "employee", "prakash@bank.com", "9876543212",
                     "prakash", "employeepass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- 4. EMPLOYEE USER (Smita Pawar, ID: 1004) --- [ADDED]
    printf("\n--- Creating EMPLOYEE (Smita Pawar) ---\n");
    if (add_employee("Smita", "Pawar", 31, "Loan_Dept", "employee", "smita@bank.com", "9876543213",
                     "smita", "employeepass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }
    
    // --- 5. CUSTOMER 1 (Priya Sawant, ID: 1005) --- [ID UPDATED]
    printf("\n--- Creating CUSTOMER 1 (Priya Sawant) ---\n");
    user_rec_t cust1_user = {0}; 
    account_rec_t cust1_acc = {0}; 
    
    strncpy(cust1_user.first_name, "Priya", sizeof(cust1_user.first_name) - 1);
    strncpy(cust1_user.last_name, "Sawant", sizeof(cust1_user.last_name) - 1);
    cust1_user.age = 30;
    strncpy(cust1_user.address, "1_Main_Street_Nagpur", sizeof(cust1_user.address) - 1);
    strncpy(cust1_user.email, "priya@guest.com", sizeof(cust1_user.email) - 1);
    strncpy(cust1_user.phone, "9000000001", sizeof(cust1_user.phone) - 1);
    
    if (add_new_customer(&cust1_user, &cust1_acc, "priya", "customerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- 6. CUSTOMER 2 (Rohan Deshmukh, ID: 1006) --- [ID UPDATED]
    printf("\n--- Creating CUSTOMER 2 (Rohan Deshmukh) ---\n");
    user_rec_t cust2_user = {0}; 
    account_rec_t cust2_acc = {0}; 
    
    strncpy(cust2_user.first_name, "Rohan", sizeof(cust2_user.first_name) - 1);
    strncpy(cust2_user.last_name, "Deshmukh", sizeof(cust2_user.last_name) - 1);
    cust2_user.age = 25;
    strncpy(cust2_user.address, "2_Bank_Road_Pune", sizeof(cust2_user.address) - 1);
    strncpy(cust2_user.email, "rohan@guest.com", sizeof(cust2_user.email) - 1);
    strncpy(cust2_user.phone, "9000000002", sizeof(cust2_user.phone) - 1);
    
    if (add_new_customer(&cust2_user, &cust2_acc, "rohan", "customerpass", resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        success_count++;
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }

    // --- CUSTOMER 3 (Ajit Kale) --- [REMOVED]
    // --- CUSTOMER 4 (Vamsi Krishna) --- [REMOVED]

    printf("\n--- Bootstrapping Complete ---\n");
    printf("%d out of 6 initial users successfully created.\n", success_count);

    if (success_count == 6) {
        printf("\nDefault Credentials for Testing:\n");
        printf("Role     | Name (ID)             | Username  | Password\n");
        printf("---------|-----------------------|-----------|-------------\n");
        printf("Admin    | Siddhesh Abhang (1001)| siddhesh  | adminpass\n");
        printf("Manager  | Manasi Joshi (1002)   | manasi    | managerpass\n");
        printf("Employee | Prakash Kadam (1003)  | prakash   | employeepass\n");
        printf("Employee | Smita Pawar (1004)    | smita     | employeepass\n");
        printf("Customer1| Priya Sawant (1005)   | priya     | customerpass\n");
        printf("Customer2| Rohan Deshmukh (1006) | rohan     | customerpass\n");
    }
    
    return 0;
}