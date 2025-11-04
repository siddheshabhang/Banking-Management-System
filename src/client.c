#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "client.h"
#include "server.h" 
#include <ctype.h> 
#include <stdbool.h> 

// --- Network Utility ---

/*
 * send_request_and_get_response
 * Handles the two-way communication with the server for a single request.
 * Manages connection error detection.
 */
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

// --- Input Handling Utilities ---

/*
 * clear_stdin
 * Clears the input buffer to prevent leftover newlines from breaking scanf.
 */
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// --- SECTION: Robust Input Validation ---

/*
 * read_validated_string
 * This is a generic, looping prompt that forces user input to pass a
 * specific validator function, which is passed as a function pointer.
 * This makes the input validation system clean and reusable.
 */
void read_validated_string(const char* prompt, char* buffer, int size, bool (*validator)(const char*, char*, size_t)) {
    char error_msg[128] = {0};
    while (true) {
        printf("%s: ", prompt);
        fgets(buffer, size, stdin);
        buffer[strcspn(buffer, "\n")] = 0; 

        if (validator(buffer, error_msg, sizeof(error_msg))) {
            break; 
        } else {
            printf("Error: %s. Please try again.\n", error_msg);
        }
    }
}

/*
 * validate_not_empty
 * Validator: Ensures the input string is not empty.
 */
bool validate_not_empty(const char* input, char* error_msg, size_t err_sz) {
    if (input[0] == '\0') {
        snprintf(error_msg, err_sz, "Input cannot be empty");
        return false;
    }
    return true;
}

/*
 * validate_name_part
 * Validator: Ensures name is not empty, not too long, and has no spaces.
 */
bool validate_name_part(const char* input, char* error_msg, size_t err_sz) {
    if (!validate_not_empty(input, error_msg, err_sz)) return false;
    if (strlen(input) >= MAX_FNAME_LEN) { 
        snprintf(error_msg, err_sz, "Input too long (max %d)", MAX_FNAME_LEN - 1);
        return false;
    }
    if (strchr(input, ' ') != NULL) {
        snprintf(error_msg, err_sz, "Input cannot contain spaces");
        return false;
    }
    return true;
}

/*
 * validate_address
 * Validator: Ensures address is not empty, not too long, and has no spaces.
 */
bool validate_address(const char* input, char* error_msg, size_t err_sz) {
    if (!validate_not_empty(input, error_msg, err_sz)) return false;
    if (strlen(input) >= MAX_ADDR_LEN) {
        snprintf(error_msg, err_sz, "Input too long (max %d)", MAX_ADDR_LEN - 1);
        return false;
    }
    if (strchr(input, ' ') != NULL) {
        snprintf(error_msg, err_sz, "Input cannot contain spaces (use '_' if needed)");
        return false;
    }
    return true;
}

/*
 * validate_email
 * Validator: Ensures email is not empty, not too long, and has '@' and '.'.
 */
bool validate_email(const char* input, char* error_msg, size_t err_sz) {
    if (!validate_not_empty(input, error_msg, err_sz)) return false;
    if (strlen(input) >= MAX_EMAIL_LEN) {
        snprintf(error_msg, err_sz, "Email too long (max %d)", MAX_EMAIL_LEN - 1);
        return false;
    }
    if (strchr(input, '@') == NULL || strchr(input, '.') == NULL) {
        snprintf(error_msg, err_sz, "Invalid email format (must contain '@' and '.')");
        return false;
    }
    return true;
}

/*
 * validate_phone
 * Validator: Ensures phone is not empty, exactly 10 digits, and all digits.
 */
bool validate_phone(const char* input, char* error_msg, size_t err_sz) {
    if (!validate_not_empty(input, error_msg, err_sz)) return false;
    if (strlen(input) != 10) {
        snprintf(error_msg, err_sz, "Phone must be exactly 10 digits");
        return false;
    }
    for (int i = 0; i < 10; i++) {
        if (!isdigit(input[i])) {
            snprintf(error_msg, err_sz, "Phone must contain only digits");
            return false;
        }
    }
    return true;
}

/*
 * validate_credential
 * Validator: Ensures username/password is not empty, not too long, no spaces.
 */
bool validate_credential(const char* input, char* error_msg, size_t err_sz) {
    if (!validate_not_empty(input, error_msg, err_sz)) return false;
    if (strlen(input) >= MAX_USERNAME_LEN) { // Use larger of user/pass
        snprintf(error_msg, err_sz, "Input too long");
        return false;
    }
    if (strchr(input, ' ') != NULL) {
        snprintf(error_msg, err_sz, "Input cannot contain spaces");
        return false;
    }
    return true;
}

/*
 * read_valid_double
 * Helper to read a valid, positive monetary amount.
 * Loops until input is valid.
 */
double read_valid_double(const char* prompt) {
    char buffer[128];
    char error_msg[128];
    while (true) {
        printf("%s: ", prompt);
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0; 
        
        if (buffer[0] == '\0') {
            printf("Error: Input cannot be empty. Please try again.\n");
            continue;
        }

        char* endptr;
        double value = strtod(buffer, &endptr);

        if (endptr == buffer || *endptr != '\0') {
            printf("Error: Invalid number. Please enter digits only (e.g., 100.50).\n");
        } else if (value <= 0) {
            printf("Error: Amount must be positive. Please try again.\n");
        } else {
            return value; 
        }
    }
}

// --- SECTION: Role-Based Menus ---

/*
 * customer_menu
 * Displays the main menu and handles I/O for the Customer role.
 */
void customer_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while (1) {
        printf("\n--- Customer Menu (Acct No: AC%d) ---\n", userId);
        printf("1. View Balance\n2. Deposit Money\n3. Withdraw Money\n4. Transfer Funds\n");
        printf("5. Apply Loan\n6. View Loan Status\n7. Add Feedback\n8. View Feedback Status\n");
        printf("9. View Transaction History\n");
        printf("10. View Personal Details\n");
        printf("11. Change Password\n");
        printf("12. Logout (Back to main menu)\n");
        printf("Enter choice: ");
        
        if (scanf("%d", &choice) != 1) {
            clear_stdin();
            printf("Invalid input. Please enter a number.\n");
            continue;
        }
        clear_stdin(); 

        memset(&req, 0, sizeof(req));
        memset(&resp, 0, sizeof(resp));

        switch (choice) {
            case 1:
                strcpy(req.op, "VIEW_BALANCE");
                snprintf(req.payload, sizeof(req.payload), "%d", userId);
                break;
            case 2:
                {
                    double amount = read_valid_double("Enter deposit amount");
                    strcpy(req.op, "DEPOSIT");
                    snprintf(req.payload, sizeof(req.payload), "%u %lf", userId, amount);
                }
                break;
            case 3:
                {
                    double amount = read_valid_double("Enter withdrawal amount");
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
                    read_validated_string("", acct_str, sizeof(acct_str), validate_not_empty);
                    if (acct_str[0] == 'A' || acct_str[0] == 'a') {
                        toId = atoi(acct_str + 2); 
                    } else {
                        toId = atoi(acct_str);
                    }
                    amount = read_valid_double("Enter amount");
                    strcpy(req.op, "TRANSFER");
                    snprintf(req.payload, sizeof(req.payload), "%u %u %lf", userId, toId, amount);
                }
                break;
            case 5:
                {
                    double loanAmount = read_valid_double("Enter loan amount");
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
                    read_validated_string("Enter feedback (max 512 chars)", feedback, sizeof(feedback), validate_not_empty);
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
                strcpy(req.op, "VIEW_DETAILS");
                snprintf(req.payload, sizeof(req.payload), "%u", userId);
                break;
            case 11:
                {
                    char newpass[MAX_PASSWORD_LEN];
                    read_validated_string("Enter new password (no spaces)", newpass, sizeof(newpass), validate_credential);
                    strcpy(req.op, "CHANGE_PASSWORD");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", userId, newpass);
                }
                break;
            case 12:
                strcpy(req.op, "LOGOUT");
                break;
            default:
                printf("Invalid choice!\n");
                continue;
        }

        send_request_and_get_response(sockfd, &req, &resp);
        printf("\n--- Server Response ---\n%s\n-----------------------\n", resp.message);

        if (resp.status_code == -1) return; 
        if (choice == 12) return; 
    }
}

/*
 * employee_menu
 * Displays the main menu and handles I/O for the Employee role.
 */
void employee_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while (1) {
        printf("\n--- Employee Menu (User: %s, ID: %d) ---\n", userName, userId);
        printf("1. Add New Customer\n2. Modify Customer Details\n3. View Pending Loans\n");
        printf("4. Approve/Reject Loans\n5. View Assigned Loans\n6. View Customer Transactions\n");
        printf("7. Change Password\n");
        printf("8. Logout (Back to main menu)\n");
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
                    char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], address[MAX_ADDR_LEN], email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN];
                    char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
                    int age;
                    read_validated_string("Enter first name (no spaces)", fname, sizeof(fname), validate_name_part);
                    read_validated_string("Enter last name (no spaces)", lname, sizeof(lname), validate_name_part);
                    printf("Enter age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    read_validated_string("Enter address (no spaces, use '_')", address, sizeof(address), validate_address);
                    read_validated_string("Enter email", email, sizeof(email), validate_email);
                    read_validated_string("Enter phone (10 digits)", phone, sizeof(phone), validate_phone);
                    read_validated_string("Enter desired username (no spaces)", username, sizeof(username), validate_credential);
                    read_validated_string("Enter desired password (no spaces)", password, sizeof(password), validate_credential);

                    strcpy(req.op, "ADD_CUSTOMER");
                    snprintf(req.payload, sizeof(req.payload), "%s %s %d %s %s %s %s %s", fname, lname, age, address, email, phone, username, password);
                }
                break;
            case 2:
                {
                    int custId;
                    char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], address[MAX_ADDR_LEN], email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN];
                    char acct_str[32];        
                    int age;
                    read_validated_string("Enter customer Account No (e.g., AC1004)", acct_str, sizeof(acct_str), validate_not_empty);
                    if (acct_str[0] == 'A' || acct_str[0] == 'a') {
                        custId = atoi(acct_str + 2);
                    } else {
                        custId = atoi(acct_str);
                    }
                    read_validated_string("Enter new first name (no spaces)", fname, sizeof(fname), validate_name_part);
                    read_validated_string("Enter new last name (no spaces)", lname, sizeof(lname), validate_name_part);
                    printf("Enter new age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    read_validated_string("Enter new address (no spaces, use '_')", address, sizeof(address), validate_address);
                    read_validated_string("Enter new email", email, sizeof(email), validate_email);
                    read_validated_string("Enter new phone (10 digits)", phone, sizeof(phone), validate_phone);
                    
                    strcpy(req.op, "MODIFY_CUSTOMER");
                    snprintf(req.payload, sizeof(req.payload), "%u %d %s %s %s %s %s", custId, age, fname, lname, address, email, phone);
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
                    read_validated_string("Enter customer Account No (e.g., AC1004)", acct_str, sizeof(acct_str), validate_not_empty);
                    if (acct_str[0] == 'A' || acct_str[0] == 'a') {
                        custId = atoi(acct_str + 2);
                    } else {
                        custId = atoi(acct_str);
                    }
                    strcpy(req.op, "VIEW_CUST_TRANSACTIONS");
                    snprintf(req.payload, sizeof(req.payload), "%u", custId);
                }
                break;
            case 7:
                {
                    char newpass[MAX_PASSWORD_LEN];
                    read_validated_string("Enter new password (no spaces)", newpass, sizeof(newpass), validate_credential);
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

/*
 * manager_menu
 * Displays the main menu and handles I/O for the Manager role.
 */
void manager_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while(1) {
        printf("\n--- Manager Menu (User: %s, ID: %d) ---\n", userName, userId);
        printf("1. Activate/Deactivate Customer Account\n2. View Non-Assigned Loans\n");
        printf("3. Assign Loan to Employee\n4. Review Customer Feedback\n");
        printf("5. Change Password\n");
        printf("6. Logout (Back to main menu)\n");
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
                    read_validated_string("Enter customer Account No (e.g., AC1004)", acct_str, sizeof(acct_str), validate_not_empty);
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
                    read_validated_string("Enter new password (no spaces)", newpass, sizeof(newpass), validate_credential);
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

        if (resp.status_code == -1) return; 
        if (choice == 6) return; 
    }
}

/*
 * admin_menu
 * Displays the main menu and handles I/O for the Admin role.
 */
void admin_menu(int userId, int sockfd, const char* userName) {
    int choice;
    request_t req;
    response_t resp;

    while(1) {
        printf("\n--- Admin Menu (User: %s, ID: %d) ---\n", userName, userId);
        printf("1. Add New Bank Employee\n2. Modify User Details\n");
        printf("3. Manage User Roles\n");
        printf("4. Change Password\n");
        printf("5. Logout (Back to main menu)\n");
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
                    char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], role[MAX_ROLE_STR], address[MAX_ADDR_LEN], email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN];
                    char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
                    int age;
                    read_validated_string("Enter first name (no spaces)", fname, sizeof(fname), validate_name_part);
                    read_validated_string("Enter last name (no spaces)", lname, sizeof(lname), validate_name_part);
                    printf("Enter age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    read_validated_string("Enter address (no spaces, use '_')", address, sizeof(address), validate_address);
                    read_validated_string("Enter role (employee/manager)", role, sizeof(role), validate_credential);
                    if(strcmp(role, "customer"))
                    {
                        printf("Error: Invalid role for employee. Must be 'employee' or 'manager'.\n");
                        continue;
                    }
                    read_validated_string("Enter email", email, sizeof(email), validate_email);
                    read_validated_string("Enter phone (10 digits)", phone, sizeof(phone), validate_phone);
                    read_validated_string("Enter desired username (no spaces)", username, sizeof(username), validate_credential);
                    read_validated_string("Enter desired password (no spaces)", password, sizeof(password), validate_credential);

                    strcpy(req.op, "ADD_EMPLOYEE");
                    snprintf(req.payload, sizeof(req.payload), "%s %s %d %s %s %s %s %s %s", fname, lname, age, address, role, email, phone, username, password);
                }
                break;
            case 2:
                {
                    int targetId;
                    char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], address[MAX_ADDR_LEN], email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN];
                    int age;
                    printf("Enter user ID to modify: ");
                    scanf("%d", &targetId);
                    clear_stdin();
                    read_validated_string("Enter new first name (no spaces)", fname, sizeof(fname), validate_name_part);
                    read_validated_string("Enter new last name (no spaces)", lname, sizeof(lname), validate_name_part);
                    printf("Enter new age: ");
                    scanf("%d", &age);
                    clear_stdin();
                    read_validated_string("Enter new address (no spaces, use '_')", address, sizeof(address), validate_address);
                    read_validated_string("Enter new email", email, sizeof(email), validate_email);
                    read_validated_string("Enter new phone (10 digits)", phone, sizeof(phone), validate_phone);
                    
                    strcpy(req.op, "MODIFY_USER");
                    snprintf(req.payload, sizeof(req.payload), "%u %d %s %s %s %s %s", targetId, age, fname, lname, address, email, phone);
                }
                break;
            case 3:
                {
                    printf("Fetching user list...\n");
                    strcpy(req.op, "LIST_USERS");
                    req.payload[0] = '\0'; 
                    
                    send_request_and_get_response(sockfd, &req, &resp);
                
                    printf("\n--- Server Response ---\n%s\n-----------------------\n", resp.message);
                    
                    if (resp.status_code != 0) {
                        continue; 
                    }
                    int targetId;
                    char role[MAX_ROLE_STR];
                    printf("Enter user ID to change role: ");
                    scanf("%d", &targetId);
                    clear_stdin();
                    read_validated_string("Enter new role (employee/manager)", role, sizeof(role), validate_credential);
                    
                    memset(&req, 0, sizeof(req)); 
                    strcpy(req.op, "CHANGE_ROLE");
                    snprintf(req.payload, sizeof(req.payload), "%u %s", targetId, role);
                }
                break;
            case 4:
                {
                    char newpass[MAX_PASSWORD_LEN];
                    read_validated_string("Enter new password (no spaces)", newpass, sizeof(newpass), validate_credential);
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

/*
 * main
 * Entry point for the client executable.
 * Handles connection and the main login/role-selection loop.
 */
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

    while(1) {
        int userId = 0; 
        char role[MAX_ROLE_STR] = {0};
        char name[MAX_FNAME_LEN] = {0}; 
        request_t req;
        response_t resp;
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
            
            char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
            read_validated_string("Username", username, sizeof(username), validate_credential);
            read_validated_string("Password", password, sizeof(password), validate_credential);
            memset(&req, 0, sizeof(req));
            strcpy(req.op, "LOGIN");
            snprintf(req.payload, sizeof(req.payload), "%s %s", username, password);
            
            send_request_and_get_response(sockfd, &req, &resp);

            if (sscanf(resp.message, "SUCCESS %d %[^|]|%[^\n]", &userId, role, name) == 3) {
                
                int valid_role = 0;
                if (choice == 1 && strcmp(role, "customer") == 0) valid_role = 1;
                if (choice == 2 && strcmp(role, "employee") == 0) valid_role = 1;
                if (choice == 3 && strcmp(role, "manager") == 0) valid_role = 1;
                if (choice == 4 && strcmp(role, "admin") == 0) valid_role = 1;

                if (valid_role) {
                    printf("\n==================================\n");
                    printf("Login successful! Welcome, %s!\n", name); 
                    
                    if(choice == 1) { 
                        printf("Account Number: AC%d\n", userId); 
                    }
                    printf("==================================\n");
                    break;
                } else {
                    printf("Login failed: Credentials are valid, but the role '%s' does not match selection.\n", role);
                    userId = 0; 
                }

            } else {
                printf("Login failed: %s\n", resp.message);
                userId = 0; 
            }
        } 

        // --- Role-Specific Menu Dispatch ---
        if (userId != 0) {
            if(strcmp(role, "customer") == 0) customer_menu(userId, sockfd, name);
            else if(strcmp(role, "employee") == 0) employee_menu(userId, sockfd, name);
            else if(strcmp(role, "manager") == 0) manager_menu(userId, sockfd, name);
            else if(strcmp(role, "admin") == 0) admin_menu(userId, sockfd, name);
            else printf("Unknown role received from server.\n");
        }

        printf("Logging out... returning to main menu.\n");
    } 
    close(sockfd);
    return 0;
}