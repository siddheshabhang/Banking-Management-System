#include "customer_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stddef.h>

// --- Transaction Modifiers (Used by atomic_update_account) ---

typedef struct {
    double amount;
} txn_data_t;

// Modifier function for deposit
int deposit_modifier(account_rec_t *acc, void *data) {
    txn_data_t *txn = (txn_data_t*)data;
    if (acc->active == STATUS_INACTIVE) return 0; // Fail if inactive
    acc->balance += txn->amount;
    return 1; // Success
}

// Modifier function for withdrawal
int withdraw_modifier(account_rec_t *acc, void *data) {
    txn_data_t *txn = (txn_data_t*)data;
    if (acc->active == STATUS_INACTIVE) return 0; // Fail if inactive
    if (acc->balance < txn->amount) return 0; // Insufficient Balance
    acc->balance -= txn->amount;
    return 1; // Success
}

// --- Public Functions (Using Atomic Updates) ---

int change_password(uint32_t user_id, const char *newpass, char *resp_msg, size_t resp_sz) {
    user_rec_t user;
    if(!read_user(user_id, &user)) {
        snprintf(resp_msg, resp_sz, "User not found");
        return 0;
    }
    
    // Generate new hash and store it
    generate_password_hash(newpass, user.password_hash, sizeof(user.password_hash));
    
    if (write_user(&user)) {
        snprintf(resp_msg, resp_sz, "Password changed successfully");
        return 1;
    } else {
        snprintf(resp_msg, resp_sz, "Password change failed (write error)");
        return 0;
    }
}

int view_balance(uint32_t user_id, char *resp_msg, size_t resp_sz) {
    account_rec_t acc;
    if(!read_account(user_id, &acc)) {
        snprintf(resp_msg, resp_sz, "Account not found");
        return 0;
    }
    snprintf(resp_msg, resp_sz, "Balance: %.2lf (Status: %s)", acc.balance, acc.active == STATUS_ACTIVE ? "Active" : "Inactive");
    return 1;
}

// FIX: Now uses atomic_update_account for thread safety
int deposit_money(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz) {
    if (amount <= 0) {
        snprintf(resp_msg, resp_sz, "Deposit amount must be positive.");
        return 0;
    }
    
    txn_data_t data = {amount};
    
    if (atomic_update_account(user_id, deposit_modifier, &data)) {
        // Log transaction
        txn_rec_t tx = {0, 0, user_id, amount, time(NULL), "deposit"};
        append_transaction(&tx);
        snprintf(resp_msg, resp_sz, "Deposit Successful: %.2lf", amount);
        return 1;
    }
    
    // Check failure reason
    account_rec_t acc;
    if (read_account(user_id, &acc) && acc.active == STATUS_INACTIVE) {
        snprintf(resp_msg, resp_sz, "Deposit Failed: Account is inactive");
    } else {
        snprintf(resp_msg, resp_sz, "Deposit Failed: Account not found");
    }
    return 0;
}

// FIX: Now uses atomic_update_account for thread safety
int withdraw_money(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz) {
     if (amount <= 0) {
        snprintf(resp_msg, resp_sz, "Withdrawal amount must be positive.");
        return 0;
    }

    txn_data_t data = {amount};

    if (atomic_update_account(user_id, withdraw_modifier, &data)) {
        // Log transaction
        txn_rec_t tx = {0, user_id, 0, amount, time(NULL), "withdraw"};
        append_transaction(&tx);
        snprintf(resp_msg, resp_sz, "Withdrawal Successful: %.2lf", amount);
        return 1;
    }
    
    // Check specific failure reason for better user feedback
    account_rec_t acc;
    if (!read_account(user_id, &acc)) snprintf(resp_msg, resp_sz, "Withdrawal Failed: Account not found");
    else if (acc.active == STATUS_INACTIVE) snprintf(resp_msg, resp_sz, "Withdrawal Failed: Account is inactive");
    else if (acc.balance < amount) snprintf(resp_msg, resp_sz, "Withdrawal Failed: Insufficient Balance (Current: %.2lf)", acc.balance);
    else snprintf(resp_msg, resp_sz, "Withdrawal Failed: Unexpected error");
    return 0;
}

// FIX: This now uses the atomic helpers.
// NOTE: This is still not a true "ACID" transaction, as the two operations
// (withdraw from A, deposit to B) are not in a single atomic block.
// A failure after sender withdrawal but before receiver deposit would lose money.
// A proper fix requires a global lock or transaction journal, which is
// outside the scope of this project's file-locking focus.
int transfer_funds(uint32_t from_id, uint32_t to_id, double amount, char *resp_msg, size_t resp_sz) {
    if (from_id == to_id) {
         snprintf(resp_msg, resp_sz, "Cannot transfer to the same account.");
         return 0;
    }
    if (amount <= 0) {
        snprintf(resp_msg, resp_sz, "Transfer amount must be positive.");
        return 0;
    }

    txn_data_t withdraw_data = {amount};
    txn_data_t deposit_data = {amount};

    // 1. Check receiver account exists and is active first
    account_rec_t to_acc;
    if (!read_account(to_id, &to_acc) || to_acc.active == STATUS_INACTIVE) {
        snprintf(resp_msg, resp_sz, "Transfer Failed: Recipient account not found or is inactive.");
        return 0;
    }

    // 2. Attempt to withdraw from sender
    if (!atomic_update_account(from_id, withdraw_modifier, &withdraw_data)) {
        // Withdrawal failed, check why
        account_rec_t from_acc;
        if (!read_account(from_id, &from_acc)) snprintf(resp_msg, resp_sz, "Transfer Failed: Sender account not found");
        else if (from_acc.active == STATUS_INACTIVE) snprintf(resp_msg, resp_sz, "Transfer Failed: Sender account is inactive");
        else if (from_acc.balance < amount) snprintf(resp_msg, resp_sz, "Transfer Failed: Insufficient Balance (Current: %.2lf)", from_acc.balance);
        else snprintf(resp_msg, resp_sz, "Transfer Failed: Sender account error");
        return 0;
    }

    // 3. Attempt to deposit to receiver
    if (!atomic_update_account(to_id, deposit_modifier, &deposit_data)) {
        // This is the critical failure point! The money is gone from sender but not in receiver.
        // We must attempt to roll back the withdrawal.
        snprintf(resp_msg, resp_sz, "CRITICAL ERROR: Transfer failed after withdrawal. Contact support.");
        
        // Rollback: Deposit the money back to the sender
        atomic_update_account(from_id, deposit_modifier, &withdraw_data);
        return 0;
    }
    
    // 4. Log transactions
    time_t now = time(NULL);
    txn_rec_t tx1 = {0, from_id, to_id, amount, now, "transfer_out"};
    append_transaction(&tx1);

    txn_rec_t tx2 = {0, from_id, to_id, amount, now, "transfer_in"};
    append_transaction(&tx2);

    snprintf(resp_msg, resp_sz, "Transfer Successful: %.2lf from %u to %u", amount, from_id, to_id);
    return 1;
}

int apply_loan(uint32_t user_id, double amount, char *resp_msg, size_t resp_sz) {
    if (amount <= 0) {
        snprintf(resp_msg, resp_sz, "Loan amount must be positive.");
        return 0;
    }
    
    // Initialize loan record
    loan_rec_t loan = {0, user_id, amount, LOAN_PENDING, 0, time(NULL), 0, ""};
    
    if(append_loan(&loan)) {
        snprintf(resp_msg, resp_sz, "Loan Application Submitted (ID: %llu)", (unsigned long long)loan.loan_id);
        return 1;
    }
    snprintf(resp_msg, resp_sz, "Loan Apply Failed (Server Error)");
    return 0;
}

// NEW FEATURE: View Loan Status
int view_loan_status(uint32_t user_id, char *resp_msg, size_t resp_sz) {
    int fd = open(LOANS_DB_FILE, O_RDONLY);
    if(fd < 0) { snprintf(resp_msg, resp_sz, "No loan records found"); return 0; }
    
    lock_file(fd);
    loan_rec_t loan;
    char tmp[512]; 
    resp_msg[0] = '\0'; // Start with an empty string
    int found = 0;
    
    const char *status_map[] = {"PENDING", "ASSIGNED", "APPROVED", "REJECTED"};

    while(read(fd, &loan, sizeof(loan_rec_t)) == sizeof(loan_rec_t)) {
        if(loan.user_id == user_id) {
            snprintf(tmp, sizeof(tmp), "ID: %llu, Amount: %.2lf, Status: %s\n",
                     (unsigned long long)loan.loan_id, loan.amount, 
                     status_map[loan.status]);
            strncat(resp_msg, tmp, resp_sz - strlen(resp_msg) - 1);
            found = 1;
        }
    }
    
    unlock_file(fd);
    close(fd);
    
    if (!found) {
        snprintf(resp_msg, resp_sz, "No loan applications found for your ID.");
    }
    return 1;
}

// NEW FEATURE: Add Feedback
int add_feedback(uint32_t user_id, const char *msg, char *resp_msg, size_t resp_sz) {
    feedback_rec_t fb = {0, user_id, "", 0, "", time(NULL)};
    strncpy(fb.message, msg, sizeof(fb.message) - 1);
    
    if(append_feedback(&fb)) {
        snprintf(resp_msg, resp_sz, "Feedback Submitted (ID: %llu). Thank you!", (unsigned long long)fb.fb_id);
        return 1;
    }
    snprintf(resp_msg, resp_sz, "Feedback submission failed.");
    return 0;
}

// NEW FEATURE: View Feedback Status
int view_feedback_status(uint32_t user_id, char *resp_msg, size_t resp_sz) {
    int fd = open(FEEDBACK_DB_FILE, O_RDONLY);
    if(fd < 0) { snprintf(resp_msg, resp_sz, "No feedback records found"); return 0; }
    
    lock_file(fd);
    feedback_rec_t fb;
    char tmp[512]; 
    resp_msg[0] = '\0';
    int found = 0;
    
    while(read(fd, &fb, sizeof(feedback_rec_t)) == sizeof(feedback_rec_t)) {
        if(fb.user_id == user_id) {
            snprintf(tmp, sizeof(tmp), "ID: %llu, Status: %s, Msg: \"%.30s...\"\n",
                     (unsigned long long)fb.fb_id, 
                     fb.reviewed ? "REVIEWED" : "PENDING",
                     fb.message);
            strncat(resp_msg, tmp, resp_sz - strlen(resp_msg) - 1);
            found = 1;
        }
    }
    
    unlock_file(fd);
    close(fd);
    
    if (!found) {
        snprintf(resp_msg, resp_sz, "No feedback submitted by your ID.");
    }
    return 1;
}

// NEW FEATURE: View Transaction History
int view_transaction_history(uint32_t user_id, char *resp_msg, size_t resp_sz) {
    int fd = open(TRANSACTIONS_DB_FILE, O_RDONLY);
    if(fd < 0) { snprintf(resp_msg, resp_sz, "No transaction history found"); return 0; }
    
    lock_file(fd);
    txn_rec_t tx;
    char tmp[256]; 
    resp_msg[0] = '\0';
    int found = 0;
    
    strncat(resp_msg, "Type        | Amount   | Other Acct | Timestamp\n", resp_sz - 1);
    strncat(resp_msg, "------------|----------|------------|-------------------\n", resp_sz - 1);

    // Read file from end to beginning to show most recent first
    off_t offset = lseek(fd, -sizeof(txn_rec_t), SEEK_END);
    while(offset >= 0) {
        if(read(fd, &tx, sizeof(txn_rec_t)) != sizeof(txn_rec_t)) break;

        if(tx.from_account == user_id || tx.to_account == user_id) {
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
        
        offset = lseek(fd, -2 * sizeof(txn_rec_t), SEEK_CUR); // Move back two records (one read + one more)
    }
    
    unlock_file(fd);
    close(fd);
    
    if (!found) {
        snprintf(resp_msg, resp_sz, "No transaction history found for your ID.");
    }
    return 1;
}