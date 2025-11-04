#include "employee_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <time.h>   
#include <fcntl.h>  
#include <unistd.h> 

/* --- EMPLOYEE MODULE LOGIC (Customer/Loan Management) --- */

int deposit_modifier(account_rec_t *acc, void *data);   // Prototype from customer_module.c
typedef struct {
    double amount;
} txn_data_t;

/* --- Modifier for approve_reject_loan (Atomic Loan Update Logic) --- */
typedef struct {
    const char *action;
    uint32_t emp_id;
    char *resp_msg;
    size_t resp_sz;
} approve_loan_data;

// Loan modification logic (Executed under loan record lock)
int approve_reject_loan_modifier(loan_rec_t *loan, void *data) {
    approve_loan_data *d = (approve_loan_data*)data;

    // Consistency Checks
    if(loan->status != LOAN_ASSIGNED) {
        snprintf(d->resp_msg, d->resp_sz, "Loan not assigned to an employee or already processed.");
        return 0; 
    }
    if(loan->assigned_to != d->emp_id) {
         snprintf(d->resp_msg, d->resp_sz, "Loan not assigned to you.");
         return 0; 
    }
    
    if(strcmp(d->action,"approve")==0) {
        loan->status = LOAN_APPROVED;
        
        /* --- CRITICAL: SEQUENTIAL ATOMIC OPERATION (Loan Approval + Deposit) --- */
        
        // 1. Check account status *before* trying to deposit
        account_rec_t cust_acc;
        if (!read_account(loan->user_id, &cust_acc) || cust_acc.active == STATUS_INACTIVE) {
            snprintf(d->resp_msg, d->resp_sz, "Loan Approval Failed: Customer account is inactive or not found.");
            return 0; 
        }
        
        // 2. Prepare the data for the *external* deposit_modifier
        txn_data_t deposit_data = {loan->amount};
        
        // 3. Perform the atomic deposit
        if (!atomic_update_account(loan->user_id, deposit_modifier, &deposit_data)) {
            snprintf(d->resp_msg, d->resp_sz, "Loan Decision Failed: Approved, but failed to deposit funds.");
            return 0; // Abort loan status change
        }

        // 4. Log the transaction (append is atomic)
        txn_rec_t tx = {0, 0, loan->user_id, loan->amount, time(NULL), "loan_deposit"};
        append_transaction(&tx);
        
    }
    else if (strcmp(d->action,"reject")==0) {
        loan->status = LOAN_REJECTED;
    }
    else {
        snprintf(d->resp_msg, d->resp_sz, "Invalid action. Use 'approve' or 'reject'.");
        return 0; 
    }
    
    loan->processed_at = time(NULL);
    snprintf(d->resp_msg, d->resp_sz, "Loan ID %llu Decision Recorded: %s", (unsigned long long)loan->loan_id, d->action);
    return 1; 
}


// approve_reject_loan (Wrapper for atomic loan update)
int approve_reject_loan(uint64_t loan_id, const char *action, uint32_t emp_id, char *resp_msg, size_t resp_sz) {
    approve_loan_data data = {action, emp_id, resp_msg, resp_sz};
    
    if (atomic_update_loan(loan_id, approve_reject_loan_modifier, &data)) {
        return 1; 
    }
    if (resp_msg[0] == '\0') {
         snprintf(resp_msg, resp_sz, "Loan Decision Failed (Loan not found or concurrency error)");
    }
    return 0;
}

/* --- Modifier for modify_customer (Atomic User Update Logic) --- */
typedef struct {
    const char *first_name;
    const char *last_name;
    int age;
    const char *address;
    const char *email;
    const char *phone;
    char *resp_msg;
    size_t resp_sz;
} modify_cust_data;

int modify_customer_modifier(user_rec_t *user, void *data) {
    modify_cust_data *d = (modify_cust_data*)data;

    if (user->role != ROLE_CUSTOMER) {
         snprintf(d->resp_msg, d->resp_sz, "Cannot modify non-customer user.");
         return 0; 
    }
    
    // Concurrency Check: Ensure uniqueness (Uses full-file lock internally)
    if (!check_uniqueness(user->username, d->email, d->phone, user->user_id, d->resp_msg, d->resp_sz)) {
        return 0; 
    }
    
    strncpy(user->first_name, d->first_name, sizeof(user->first_name) - 1);
    strncpy(user->last_name, d->last_name, sizeof(user->last_name) - 1);
    user->age = d->age;
    strncpy(user->address, d->address, sizeof(user->address) - 1);
    strncpy(user->email, d->email, sizeof(user->email) - 1);
    strncpy(user->phone, d->phone, sizeof(user->phone) - 1);
    snprintf(d->resp_msg, d->resp_sz, "Customer Modified (ID: %u).\nNew Details:\nName: %s %s\nAge: %d\nAddress: %s\nEmail: %s\nPhone: %s", user->user_id, user->first_name, user->last_name, user->age, user->address, user->email, user->phone);
    return 1; 
}

// modify_customer (Wrapper for atomic user update)
int modify_customer(uint32_t user_id, const char *first_name, const char *last_name, int age, const char *address, const char *email, const char *phone, char *resp_msg, size_t resp_sz) {
    modify_cust_data data = {first_name, last_name, age, address, email, phone, resp_msg, resp_sz};
    if (atomic_update_user(user_id, modify_customer_modifier, &data)) {
        return 1;
    }
    
    if (resp_msg[0] == '\0') {
         snprintf(resp_msg, resp_sz, "Customer Modification Failed (User not found or concurrency error)");
    }
    return 0;
}

// add_new_customer (Atomic creation of user and account)
int add_new_customer(user_rec_t *user, account_rec_t *acc, const char *username, const char *password, char *resp_msg, size_t resp_sz) {
    if (!check_uniqueness(username, user->email, user->phone, 0, resp_msg, resp_sz)) {
        return 0;   
    }
    user->user_id = generate_new_userId();
    user->role = ROLE_CUSTOMER;
    user->active = STATUS_ACTIVE;
    user->created_at = time(NULL);
    
    strncpy(user->username, username, sizeof(user->username) - 1);
    generate_password_hash(password, user->password_hash, sizeof(user->password_hash));

    acc->user_id = user->user_id;
    acc->account_id = user->user_id; 
    acc->balance = 0;
    acc->active = STATUS_ACTIVE;
    
    if (write_user(user) && write_account(acc)) {
        snprintf(resp_msg, resp_sz, "Customer Added (ID: %u, Username: %s)", user->user_id, user->username);
        return 1;
    }
    
    snprintf(resp_msg, resp_sz, "Customer Add Failed (Persistence Error)");
    return 0;
}

// view_assigned_loans (Read-only list)
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

// process_loans (View unassigned loans - Read-only list)
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

// view_customer_transactions (Auditing tool - Read-only list, reads backward)
int view_customer_transactions(uint32_t custId, char *resp_msg, size_t resp_sz) {
    
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

    off_t offset = lseek(fd, -sizeof(txn_rec_t), SEEK_END);
    while(offset >= 0) {
        if(read(fd, &tx, sizeof(txn_rec_t)) != sizeof(txn_rec_t)) break;
        
       char time_str[64];
        struct tm *tm_info = localtime(&tx.timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        char type_str[16];
        uint32_t other_id = 0;
        int tx_is_relevant = 0;

        if (strcmp(tx.narration, "deposit") == 0 && tx.to_account == custId) {
            strcpy(type_str, "DEPOSIT");
            other_id = tx.from_account;
            tx_is_relevant = 1;
        } else if (strcmp(tx.narration, "withdraw") == 0 && tx.from_account == custId) {
            strcpy(type_str, "WITHDRAW");
            other_id = tx.to_account;
            tx_is_relevant = 1;
        } else if (strcmp(tx.narration, "transfer_out") == 0 && tx.from_account == custId) {
            strcpy(type_str, "TRANSFER_OUT");
            other_id = tx.to_account;
            tx_is_relevant = 1;
        } else if (strcmp(tx.narration, "transfer_in") == 0 && tx.to_account == custId) {
            strcpy(type_str, "TRANSFER_IN");
            other_id = tx.from_account;
            tx_is_relevant = 1;
        } else if (strcmp(tx.narration, "loan_deposit") == 0 && tx.to_account == custId) {
            strcpy(type_str, "LOAN_DEPOSIT");
            other_id = 0; 
            tx_is_relevant = 1;
        }

        if(tx_is_relevant) {
            found = 1;
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