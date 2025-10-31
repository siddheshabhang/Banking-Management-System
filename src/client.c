#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "client.h"
#include "server.h" // For request_t, response_t, and constants

// Helper to send request and get response
// CRITICAL FIX: This now sends/receives the correct structs
void send_request_and_get_response(int sockfd, request_t *req, response_t *resp) {
    if (write(sockfd, req, sizeof(request_t)) <= 0) {
        perror("write");
        snprintf(resp->message, sizeof(resp->message), "Connection to server lost (write).");
        resp->status_code = -1;
        return;
    }
    
    if (read(sockfd, resp, sizeof(response_t)) <= 0) {
        perror("read");
        snprintf(resp->message, sizeof(resp->message), "Connection to server lost (read).");
        resp->status_code = -1;
        return;
    }
}

// Helper to clear stdin buffer
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Helper to read a line safely
void read_line(char *buffer, int size) {
    fgets(buffer, size, stdin);
    buffer[strcspn(buffer, "\n")] = 0; // Remove trailing newline
}

// Customer Menu
void customer_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while (1) {
        printf("\n--- Customer Menu (Acct No: AC%d) ---\n", userId);
        printf("1. View Balance\n2. Deposit Money\n3. Withdraw Money\n4. Transfer Funds\n");
        printf("5. Apply Loan\n6. View Loan Status\n7. Add Feedback\n8. View Feedback Status\n");
        printf("9. View Transaction History\n10. Change Password\n11. Logout\n");
        printf("Enter choice: ");
        
        if (scanf("%d", &choice) != 1) {
            clear_stdin();
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        clear_stdin(); // clear newline

        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));

        switch (choice) {
            case 1:
                strcpy(req.op, "VIEW_BALANCE");
                snprintf(req.payload, sizeof(req.payload), "%d", userId);
                break;
            case 2:
                {
                    double amount;
                    printf("Enter deposit amount: ");
                    scanf("%lf", &amount);
                    clear_stdin();
                    strcpy(req.op, "DEPOSIT");
                    snprintf(req.payload, sizeof(req.payload), "%u %lf", userId, amount);
                }
                break;
            case 3:
                {
                    double amount;
                    printf("Enter withdrawal amount: ");
                    scanf("%lf", &amount);
                    clear_stdin();
                    strcpy(req.op, "WITHDRAW");
                    snprintf(req.payload, sizeof(req.payload), "%u %lf", userId, amount);
                }
                break;
            case 4:
                {
                    int toId;
                    double amount;
                    char acct_str[32];
                    printf("Enter recipient Account No (e.g., AC1005): ");
                    read_line(acct_str, sizeof(acct_str));
                    if (acct_str[0] == 'A' || acct_str[0] == 'a') {
                        toId = atoi(acct_str + 2); 
                    } else {
                        toId = atoi(acct_str);
                    }
                    printf("Enter amount: ");
                    scanf("%lf", &amount);
                    clear_stdin();
                    strcpy(req.op, "TRANSFER");
                    snprintf(req.payload, sizeof(req.payload), "%u %u %lf", userId, toId, amount);
                }
                break;
            case 5:
                {
                    double loanAmount;
                    printf("Enter loan amount: ");
                    scanf("%lf", &loanAmount);
                    clear_stdin();
                    strcpy(req.op, "APPLY_LOAN");
                    snprintf(req.payload, sizeof(req.payload), "%u %lf", userId, loanAmount);
                }
                break;
            case 6:
                strcpy(req.op, "VIEW_LOAN");
                snprintf(req.payload, sizeof(req.payload), "%u", userId);
                break;
            case 7:
                {
                    char feedback[512];
                    printf("Enter feedback (max 512 chars): ");
                    read_line(feedback, sizeof(feedback));
                    strcpy(req.op, "ADD_FEEDBACK");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", userId, feedback);
                }
                break;
            case 8:
                strcpy(req.op, "VIEW_FEEDBACK");
                snprintf(req.payload, sizeof(req.payload), "%u", userId);
                break;
            case 9:
                strcpy(req.op, "VIEW_TRANSACTIONS");
                snprintf(req.payload, sizeof(req.payload), "%u", userId);
                break;
            case 10:
                {
                    char newpass[MAX_PASSWORD_LEN];
                    printf("Enter new password: ");
                    read_line(newpass, sizeof(newpass));
                    strcpy(req.op, "CHANGE_PASSWORD");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", userId, newpass);
                }
                break;
            case 11:
                strcpy(req.op, "LOGOUT");
                break;
            default:
                printf("Invalid choice!\n");
                continue;
        }

        send_request_and_get_response(sockfd, &req, &resp);
        printf("\n--- Server Response ---\n%s\n-----------------------\n", resp.message);

        if (resp.status_code == -1) return; // Server connection lost
        if (choice == 11) return; // Logout
    }
}

// Employee Menu
void employee_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while (1) {
        printf("\n--- Employee Menu (User: %s, ID: %d) ---\n", userName, userId);
        printf("1. Add New Customer\n2. Modify Customer Details\n3. View Pending Loans\n");
        printf("4. Approve/Reject Loans\n5. View Assigned Loans\n6. View Customer Transactions\n");
        printf("7. Change Password\n8. Logout\n");
        printf("Enter choice: ");
        
        if (scanf("%d", &choice) != 1) {
            clear_stdin();
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        clear_stdin();

        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));

        switch(choice) {
            case 1:
                {
                    char name[MAX_NAME_LEN], address[MAX_ADDR_LEN], username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
                    int age;
                    printf("Enter name (no spaces): ");
                    read_line(name, sizeof(name));
                    printf("Enter age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    printf("Enter address (no spaces): ");
                    read_line(address, sizeof(address));
                    printf("Enter desired username: ");
                    read_line(username, sizeof(username));
                    printf("Enter desired password: ");
                    read_line(password, sizeof(password));

                    strcpy(req.op, "ADD_CUSTOMER");
                    snprintf(req.payload, sizeof(req.payload), "%s %d %s %s %s", name, age, address, username, password);
                }
                break;
            case 2:
                {
                    int custId;
                    char name[MAX_NAME_LEN], address[MAX_ADDR_LEN];
                    char acct_str[32];
                    int age;
                    printf("Enter customer Account No (e.g., AC1004): ");
                    read_line(acct_str, sizeof(acct_str));
                    if (acct_str[0] == 'A' || acct_str[0] == 'a') {
                        custId = atoi(acct_str + 2);
                    } else {
                        custId = atoi(acct_str);
                    }
                    clear_stdin();
                    printf("Enter new name (no spaces): ");
                    read_line(name, sizeof(name));
                    printf("Enter new age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    printf("Enter new address (no spaces): ");
                    read_line(address, sizeof(address));
                    
                    strcpy(req.op, "MODIFY_CUSTOMER");
                    snprintf(req.payload, sizeof(req.payload), "%u %d %s %s", custId, age, name, address);
                }
                break;
            case 3:
                strcpy(req.op, "PROCESS_LOANS");
                break;
            case 4:
                {
                    unsigned long long loanId;
                    char action_str[32];
                    int action_code;
                    printf("Enter loan ID: ");
                    scanf("%llu", &loanId);
                    printf("Approve (1) / Reject (0): ");
                    scanf("%d", &action_code);
                    clear_stdin();
                    
                    if (action_code == 1) strcpy(action_str, "approve");
                    else if (action_code == 0) strcpy(action_str, "reject");
                    else { printf("Invalid action code.\n"); continue; }
                    
                    strcpy(req.op, "APPROVE_REJECT_LOAN");
                    snprintf(req.payload, sizeof(req.payload), "%llu %s %u", loanId, action_str, userId);
                }
                break;
            case 5:
                strcpy(req.op, "VIEW_ASSIGNED_LOANS");
                snprintf(req.payload, sizeof(req.payload), "%u", userId);
                break;
            case 6:
                {
                    int custId;
                    char acct_str[32];
                    printf("Enter customer Account No (e.g., AC1004): ");
                    read_line(acct_str, sizeof(acct_str));
                    if (acct_str[0] == 'A' || acct_str[0] == 'a') {
                        custId = atoi(acct_str + 2);
                    } else {
                        custId = atoi(acct_str);
                    }
                    clear_stdin();
                    strcpy(req.op, "VIEW_CUST_TRANSACTIONS");
                    snprintf(req.payload, sizeof(req.payload), "%u", custId);
                }
                break;
            case 7:
                {
                    char newpass[MAX_PASSWORD_LEN];
                    printf("Enter new password: ");
                    read_line(newpass, sizeof(newpass));
                    strcpy(req.op, "CHANGE_PASSWORD");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", userId, newpass);
                }
                break;
            case 8:
                strcpy(req.op, "LOGOUT");
                break;
            default:
                printf("Invalid choice!\n");
                continue;
        }

        send_request_and_get_response(sockfd, &req, &resp);
        printf("\n--- Server Response ---\n%s\n-----------------------\n", resp.message);
        
        if (resp.status_code == -1) return; // Server connection lost
        if (choice == 8) return; // Logout
    }
}

// Manager Menu
void manager_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while(1) {
        printf("\n--- Manager Menu (User: %s, ID: %d) ---\n", userName, userId);
        printf("1. Activate/Deactivate Customer Account\n2. View Non-Assigned Loans\n");
        printf("3. Assign Loan to Employee\n4. Review Customer Feedback\n");
        printf("5. Change Password\n6. Logout\n");
        printf("Enter choice: ");
        
        if (scanf("%d", &choice) != 1) {
            clear_stdin();
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        clear_stdin();

        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));

        switch(choice) {
            case 1:
                {
                    int custId, status;
                    char acct_str[32];
                    printf("Enter customer Account No (e.g., AC1004): ");
                    read_line(acct_str, sizeof(acct_str));
                    if (acct_str[0] == 'A' || acct_str[0] == 'a') {
                        custId = atoi(acct_str + 2);
                    } else {
                        custId = atoi(acct_str);
                    }
                    printf("Activate (1) / Deactivate (0): ");
                    scanf("%d", &status);
                    clear_stdin();
                    strcpy(req.op, "SET_ACCOUNT_STATUS");
                    snprintf(req.payload, sizeof(req.payload), "%u %u", custId, status);
                }
                break;
            case 2:
                strcpy(req.op, "VIEW_NON_ASSIGNED_LOANS");
                break;
            case 3:
                {
                    int loanId, empId;
                    printf("Enter loan ID: ");
                    scanf("%d", &loanId);
                    printf("Enter employee ID: ");
                    scanf("%d", &empId);
                    clear_stdin();
                    strcpy(req.op, "ASSIGN_LOAN");
                    snprintf(req.payload, sizeof(req.payload), "%u %u", loanId, empId);
                }
                break;
            case 4:
                strcpy(req.op, "REVIEW_FEEDBACK");
                break;
            case 5:
                {
                    char newpass[MAX_PASSWORD_LEN];
                    printf("Enter new password: ");
                    read_line(newpass, sizeof(newpass));
                    strcpy(req.op, "CHANGE_PASSWORD");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", userId, newpass);
                }
                break;
            case 6:
                strcpy(req.op, "LOGOUT");
                break;
            default:
                printf("Invalid choice!\n");
                continue;
        }

        send_request_and_get_response(sockfd, &req, &resp);
        printf("\n--- Server Response ---\n%s\n-----------------------\n", resp.message);

        if (resp.status_code == -1) return; // Server connection lost
        if (choice == 6) return; // Logout
    }
}

// Admin Menu
void admin_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while(1) {
        printf("\n--- Admin Menu (User: %s, ID: %d) ---\n", userName, userId);
        printf("1. Add New Bank Employee\n2. Modify User Details\n");
        printf("3. Manage User Roles\n4. Change Password\n5. Logout\n");
        printf("Enter choice: ");
        
        if (scanf("%d", &choice) != 1) {
            clear_stdin();
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        clear_stdin();

        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));

        switch(choice) {
            case 1:
                {
                    char name[MAX_NAME_LEN], role[MAX_ROLE_STR], address[MAX_ADDR_LEN], username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
                    int age;
                    printf("Enter name (no spaces): ");
                    read_line(name, sizeof(name));
                    printf("Enter age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    printf("Enter address (no spaces): ");
                    read_line(address, sizeof(address));
                    printf("Enter role (employee/manager/admin): ");
                    read_line(role, sizeof(role));
                    printf("Enter desired username: ");
                    read_line(username, sizeof(username));
                    printf("Enter desired password: ");
                    read_line(password, sizeof(password));

                    strcpy(req.op, "ADD_EMPLOYEE");
                    snprintf(req.payload, sizeof(req.payload), "%s %d %s %s %s %s", name, age, address, role, username, password);
                }
                break;
            case 2:
                {
                    int targetId;
                    char name[MAX_NAME_LEN], address[MAX_ADDR_LEN];
                    int age;
                    printf("Enter user ID to modify: ");
                    scanf("%d", &targetId);
                    clear_stdin();
                    printf("Enter new name (no spaces): ");
                    read_line(name, sizeof(name));
                    printf("Enter new age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    printf("Enter new address (no spaces): ");
                    read_line(address, sizeof(address));
                    
                    strcpy(req.op, "MODIFY_USER");
                    snprintf(req.payload, sizeof(req.payload), "%u %d %s %s", targetId, age, name, address);
                }
                break;
            case 3:
                {
                    printf("Fetching user list...\n");
                    strcpy(req.op, "LIST_USERS");
                    req.payload[0] = '\0'; // No payload needed
                    
                    send_request_and_get_response(sockfd, &req, &resp);
                    
                    // Print the list received from the server
                    printf("\n--- Server Response ---\n%s\n-----------------------\n", resp.message);
                    
                    if (resp.status_code != 0) {
                        // If fetching list failed, don't continue to the next step
                        continue; // Go back to menu
                    }
                    int targetId;
                    char role[MAX_ROLE_STR];
                    printf("Enter user ID to change role: ");
                    scanf("%d", &targetId);
                    clear_stdin();
                    printf("Enter new role (customer/employee/manager/admin): ");
                    read_line(role, sizeof(role));
                    
                    // Reset req struct for the *next* operation
                    memset(&req, 0, sizeof(req)); 
                    strcpy(req.op, "CHANGE_ROLE");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", targetId, role);
                }
                break;
            case 4:
                {
                    char newpass[MAX_PASSWORD_LEN];
                    printf("Enter new password: ");
                    read_line(newpass, sizeof(newpass));
                    strcpy(req.op, "CHANGE_PASSWORD");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", userId, newpass);
                }
                break;
            case 5:
                strcpy(req.op, "LOGOUT");
                break;
            default:
                printf("Invalid choice!\n");
                continue;
        }

        send_request_and_get_response(sockfd, &req, &resp);
        printf("\n--- Server Response ---\n%s\n-----------------------\n", resp.message);
        
        if (resp.status_code == -1) return; // Server connection lost
        if (choice == 5) return; // Logout
    }
}

// Main function
int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Usage: %s <server-ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_PORT);
    if(inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    printf("\n\t***** WELCOME TO THE BANK! *****\n");
    printf("Connected to server at %s\n", argv[1]);

    int userId = 0; // Initialize userId outside the loop
    char role[MAX_ROLE_STR] = {0};
    char name[MAX_NAME_LEN] = {0};
    request_t req;
    response_t resp;

    // --- Role Selection and Login Loop ---
    while (userId == 0) {
        printf("\n==================================\n");
        printf("Please select your role to login:\n");
        printf("1. Customer\n");
        printf("2. Employee\n");
        printf("3. Manager\n");
        printf("4. Administrator\n");
        printf("5. Exit Client\n");
        printf("Enter choice (1-5): ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            clear_stdin();
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        clear_stdin();

        if (choice == 5) {
            printf("Exiting client...\n");
            close(sockfd);
            return 0;
        }
        
        // --- Standard Login Prompt ---
        char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
        printf("Username: ");
        read_line(username, sizeof(username));
        printf("Password: ");
        read_line(password, sizeof(password));

        // Prepare LOGIN Request
        memset(&req, 0, sizeof(req));
        strcpy(req.op, "LOGIN");
        snprintf(req.payload, sizeof(req.payload), "%s %s", username, password);
        
        send_request_and_get_response(sockfd, &req, &resp);

        // Check Server Response and Validate Role
        if (sscanf(resp.message, "SUCCESS %d %[^|]|%[^\n]", &userId, role, name) == 3) {
            
            // Check if the role matches the selected choice (optional but good practice)
            int valid_role = 0;
            if (choice == 1 && strcmp(role, "customer") == 0) valid_role = 1;
            if (choice == 2 && strcmp(role, "employee") == 0) valid_role = 1;
            if (choice == 3 && strcmp(role, "manager") == 0) valid_role = 1;
            if (choice == 4 && strcmp(role, "admin") == 0) valid_role = 1;

            if (valid_role) {
                printf("\n==================================\n");
                printf("Login successful! Welcome, %s!\n", name); // <-- WELCOME MESSAGE
                
                if(choice == 1) { // If they are a customer
                    printf("Account Number: AC%d\n", userId); // <-- ACCOUNT NUMBER
                }
                printf("==================================\n");
                break;
            } else {
                printf("Login failed: Credentials are valid, but the role '%s' does not match selection.\n", role);
                userId = 0; // Reset to force re-login
            }

        } else {
            printf("Login failed: %s\n", resp.message);
            userId = 0; // Keep looping
        }
    } // End of while (userId == 0) loop

    // --- Role-Specific Menu Dispatch (Starts here) ---
    if (userId != 0) {
        if(strcmp(role, "customer") == 0) customer_menu(userId, sockfd, name);
        else if(strcmp(role, "employee") == 0) employee_menu(userId, sockfd, name);
        else if(strcmp(role, "manager") == 0) manager_menu(userId, sockfd, name);
        else if(strcmp(role, "admin") == 0) admin_menu(userId, sockfd, name);
        else printf("Unknown role received from server.\n");
    }

    printf("Logging out and disconnecting.\n");
    close(sockfd);
    return 0;
}