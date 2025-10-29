#include "employee_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>

typedef struct { double amount; } txn_data_t;
int deposit_modifier(account_rec_t *acc, void *data);

// FIX: Added required fields for User creation
int add_new_customer(user_rec_t *user, account_rec_t *acc, const char *username, const char *password, char *resp_msg, size_t resp_sz) {
    
    user->user_id = generate_new_userId();
    user->role = ROLE_CUSTOMER;
    user->active = STATUS_ACTIVE;
    user->created_at = time(NULL);
    
    // Copy username
    strncpy(user->username, username, sizeof(user->username) - 1);

    // CRITICAL SECURITY FIX: Hash the password
    generate_password_hash(password, user->password_hash, sizeof(user->password_hash));

    // Account setup
    acc->user_id = user->user_id;
    acc->account_id = user->user_id; // Use same ID for simplicity
    acc->balance = 0;
    acc->active = STATUS_ACTIVE;
    acc->created_at = time(NULL);
    
    if (write_user(user) && write_account(acc)) {
        snprintf(resp_msg, resp_sz, "Customer Added (ID: %u, Username: %s)", user->user_id, user->username);
        return 1;
    }
    
    snprintf(resp_msg, resp_sz, "Customer Add Failed (Persistence Error)");
    return 0;
}

int modify_customer(uint32_t user_id, const char *name, int age, const char *address, char *resp_msg, size_t resp_sz) {
    user_rec_t user;
    if(!read_user(user_id, &user)) {
        snprintf(resp_msg, resp_sz, "User not found");
        return 0;
    }
    
    // Only modify customer accounts
    if (user.role != ROLE_CUSTOMER) {
         snprintf(resp_msg, resp_sz, "Cannot modify non-customer user.");
         return 0;
    }

    strncpy(user.name, name, sizeof(user.name) - 1);
    user.age = age;
    strncpy(user.address, address, sizeof(user.address) - 1);
    
    if(write_user(&user)) {
        // FIX: Improved output format using correct struct fields
        snprintf(resp_msg, resp_sz, 
                 "Customer Modified (ID: %u).\nNew Details:\nName: %s\nAge: %d\nAddress: %s", 
                 user_id, user.name, user.age, user.address);
        return 1;
    }
    snprintf(resp_msg, resp_sz, "Customer Modification Failed");
    return 0;
}

int approve_reject_loan(uint64_t loan_id, const char *action, uint32_t emp_id, char *resp_msg, size_t resp_sz) {
    loan_rec_t loan;
    if(!read_loan(loan_id, &loan)) {
        snprintf(resp_msg, resp_sz, "Loan not found");
        return 0;
    }
    if(loan.status != LOAN_ASSIGNED) {
        snprintf(resp_msg, resp_sz, "Loan not assigned to an employee or already processed.");
        return 0;
    }
    if(loan.assigned_to != emp_id) {
         snprintf(resp_msg, resp_sz, "Loan not assigned to you.");
         return 0;
    }
    
    if(strcmp(action,"approve")==0) {
        loan.status = LOAN_APPROVED;
        
        // CRITICAL FIX: Deposit the loan amount into the customer's account
        txn_data_t data = {loan.amount};
        
        // This relies on atomic_update_account being thread-safe
        if (atomic_update_account(loan.user_id, deposit_modifier, &data)) {
            // Log the transaction
            txn_rec_t tx = {0, 0, loan.user_id, loan.amount, time(NULL), "loan_deposit"};
            append_transaction(&tx);
        } else {
            // Failed to deposit funds (e.g., account file error) - CRITICAL ERROR
            snprintf(resp_msg, resp_sz, "Loan Decision Failed: Approved, but failed to deposit funds.");
            return 0; 
        }
    }
    else if (strcmp(action,"reject")==0) {
        loan.status = LOAN_REJECTED;
    }
    else {
        snprintf(resp_msg, resp_sz, "Invalid action. Use 'approve' or 'reject'.");
        return 0;
    }
    
    loan.processed_at = time(NULL);
    
    if(write_loan(&loan)) {
        snprintf(resp_msg, resp_sz, "Loan ID %llu Decision Recorded: %s", (unsigned long long)loan_id, action);
        return 1;
    }
    
    snprintf(resp_msg, resp_sz, "Loan Decision Failed (Write Error)");
    return 0;
}

// NEW FEATURE: View Assigned Loans (Option 5)
int view_assigned_loans(uint32_t emp_id, char *resp_msg, size_t resp_sz) {
    int fd = open(LOANS_DB_FILE, O_RDONLY);
    if(fd<0) { snprintf(resp_msg,resp_sz,"No loans file found"); return 0; }
    lock_file(fd);
    
    loan_rec_t loan;
    char tmp[256]; 
    resp_msg[0]='\0';
    int found = 0;
    
    strncat(resp_msg, "ID   | User ID | Amount   | Status\n", resp_sz - 1);
    strncat(resp_msg, "---- | ------- | -------- | --------\n", resp_sz - 1);
    const char *status_map[] = {"PENDING", "ASSIGNED", "APPROVED", "REJECTED"};

    while(read(fd,&loan,sizeof(loan_rec_t))==sizeof(loan_rec_t)) {
        // Show loans assigned to this employee that are still marked as "ASSIGNED"
        if(loan.assigned_to == emp_id && loan.status == LOAN_ASSIGNED) {
            snprintf(tmp,sizeof(tmp),"%-4llu | %-7u | %-8.2f | %s\n",
                     (unsigned long long)loan.loan_id, loan.user_id, loan.amount, status_map[loan.status]);
            strncat(resp_msg,tmp,resp_sz-strlen(resp_msg)-1);
            found = 1;
        }
    }
    unlock_file(fd); 
    close(fd);
    
    if (!found) {
        snprintf(resp_msg, resp_sz, "No loan applications currently assigned to you.");
    }
    return 1;
}

// NEW FEATURE: Process Loans (Option 3 - View PENDING loans)
int process_loans(char *resp_msg, size_t resp_sz) {
    int fd = open(LOANS_DB_FILE, O_RDONLY);
    if(fd<0) { snprintf(resp_msg,resp_sz,"No loans file found"); return 0; }
    lock_file(fd);
    
    loan_rec_t loan;
    char tmp[256]; 
    resp_msg[0]='\0';
    int found = 0;
    
    strncat(resp_msg, "--- Pending Loan Applications (Unassigned) ---\n", resp_sz - 1);
    strncat(resp_msg, "ID   | User ID | Amount\n", resp_sz - 1);
    strncat(resp_msg, "---- | ------- | --------\n", resp_sz - 1);

    while(read(fd,&loan,sizeof(loan_rec_t))==sizeof(loan_rec_t)) {
        if(loan.status == LOAN_PENDING) {
            snprintf(tmp,sizeof(tmp),"%-4llu | %-7u | %.2f\n",
                     (unsigned long long)loan.loan_id, loan.user_id, loan.amount);
            strncat(resp_msg,tmp,resp_sz-strlen(resp_msg)-1);
            found = 1;
        }
    }
    unlock_file(fd); 
    close(fd);
    
    if (!found) {
        snprintf(resp_msg, resp_sz, "No pending loan applications available to process.");
    }
    return 1;
}

// NEW FEATURE: View Customer Transactions (Option 6)
// This is a wrapper for the customer's view_transaction_history function
int view_customer_transactions(uint32_t custId, char *resp_msg, size_t resp_sz) {
    
    // Check if customer exists
    user_rec_t user;
    if (!read_user(custId, &user) || user.role != ROLE_CUSTOMER) {
        snprintf(resp_msg, resp_sz, "Customer ID %u not found.", custId);
        return 0;
    }

    int fd = open(TRANSACTIONS_DB_FILE, O_RDONLY);
    if(fd<0) { snprintf(resp_msg,resp_sz,"No transaction history found for customer %u", custId); return 0; }
    
    lock_file(fd);
    txn_rec_t tx;
    char tmp[256]; 
    resp_msg[0] = '\0';
    int found = 0;
    
    snprintf(resp_msg, resp_sz, "--- Transaction History for Customer %u ---\n", custId);
    strncat(resp_msg, "Type        | Amount   | Other Acct | Timestamp\n", resp_sz - strlen(resp_msg) - 1);
    strncat(resp_msg, "------------|----------|------------|-------------------\n", resp_sz - strlen(resp_msg) - 1);

    // Read file from end to beginning to show most recent first
    off_t offset = lseek(fd, -sizeof(txn_rec_t), SEEK_END);
    while(offset >= 0) {
        if(read(fd, &tx, sizeof(txn_rec_t)) != sizeof(txn_rec_t)) break;
        
        if(tx.from_account == custId || tx.to_account == custId) {
            found = 1;
            char time_str[64];
            struct tm *tm_info = localtime(&tx.timestamp);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            
            char type_str[16];
            uint32_t other_id = 0;

            if (strcmp(tx.narration, "deposit") == 0) {
                strcpy(type_str, "DEPOSIT");
                other_id = tx.from_account;
            } else if (strcmp(tx.narration, "withdraw") == 0) {
                strcpy(type_str, "WITHDRAW");
                other_id = tx.to_account;
            } else if (strcmp(tx.narration, "transfer_out") == 0) {
                strcpy(type_str, "TRANSFER_OUT");
                other_id = tx.to_account;
            } else if (strcmp(tx.narration, "transfer_in") == 0) {
                strcpy(type_str, "TRANSFER_IN");
                other_id = tx.from_account;
            }

            snprintf(tmp, sizeof(tmp), "%-11s | %-8.2f | %-10u | %s\n",
                     type_str, tx.amount, other_id, time_str);
            strncat(resp_msg, tmp, resp_sz - strlen(resp_msg) - 1);
        }
        
        offset = lseek(fd, -2 * sizeof(txn_rec_t), SEEK_CUR);
    }
    
    unlock_file(fd);
    close(fd);
    
    if (!found) { 
        snprintf(resp_msg, resp_sz, "No transaction history found for customer %u.", custId);
    }
    return 1;
}