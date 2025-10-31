

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "server.h" // Includes all struct and file path definitions

/* --- Helper function to print time neatly --- */
void print_timestamp(time_t t, const char* label) {
    if (t == 0) {
        printf("%s: (not set)\n", label);
        return;
    }
    char time_buf[64];
    strncpy(time_buf, ctime(&t), sizeof(time_buf) - 1);
    time_buf[strcspn(time_buf, "\n")] = 0; // Remove trailing newline
    printf("%s: %s\n", label, time_buf);
}

/* --- Helper for Role Enum --- */
const char* get_role_str(role_t role) {
    switch(role) {
        case ROLE_CUSTOMER: return "CUSTOMER";
        case ROLE_EMPLOYEE: return "EMPLOYEE";
        case ROLE_MANAGER:  return "MANAGER";
        case ROLE_ADMIN:    return "ADMIN";
        default:            return "UNKNOWN";
    }
}

/* --- Helper for Loan Status Enum --- */
const char* get_loan_status_str(loan_status_t status) {
    switch(status) {
        case LOAN_PENDING:  return "PENDING";
        case LOAN_ASSIGNED: return "ASSIGNED";
        case LOAN_APPROVED: return "APPROVED";
        case LOAN_REJECTED: return "REJECTED";
        default:            return "UNKNOWN";
    }
}


/* --- Main Dump Functions --- */

void print_users() {
    printf("\n==========================================\n");
    printf("  DUMPING USERS (from %s)\n", USERS_DB_FILE);
    printf("==========================================\n");

    int fd = open(USERS_DB_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Could not open users.db");
        return;
    }

    user_rec_t user;
    int count = 1;
    while (read(fd, &user, sizeof(user_rec_t)) == sizeof(user_rec_t)) {
        printf("\n--- User Record %d ---\n", count++);
        printf("  User ID:      %u\n", user.user_id);
        printf("  Username:     %s\n", user.username);
        printf("  Password Hash:%s\n", user.password_hash);
        printf("  Role:         %s (%d)\n", get_role_str(user.role), user.role);
        printf("  Name:         %s %s\n", user.first_name, user.last_name);
        printf("  Age:          %u\n", user.age);
        printf("  Address:      %s\n", user.address);
        printf("  Email:        %s\n", user.email);
        printf("  Phone:        %s\n", user.phone);
        printf("  User Status:  %s\n", user.active == STATUS_ACTIVE ? "ACTIVE" : "INACTIVE");
        print_timestamp(user.created_at, "  Created At");
    }
    close(fd);
}

void print_accounts() {
    printf("\n==========================================\n");
    printf("  DUMPING ACCOUNTS (from %s)\n", ACCOUNTS_DB_FILE);
    printf("==========================================\n");

    int fd = open(ACCOUNTS_DB_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Could not open accounts.db");
        return;
    }

    account_rec_t acc;
    int count = 1;
    while (read(fd, &acc, sizeof(account_rec_t)) == sizeof(account_rec_t)) {
        printf("\n--- Account Record %d ---\n", count++);
        printf("  Account ID:   %u\n", acc.account_id);
        printf("  User ID:      %u\n", acc.user_id);
        printf("  Balance:      %.2f\n", acc.balance);
        printf("  Acct Status:  %s\n", acc.active == STATUS_ACTIVE ? "ACTIVE" : "INACTIVE");
    }
    close(fd);
}

void print_transactions() {
    printf("\n==========================================\n");
    printf("  DUMPING TRANSACTIONS (from %s)\n", TRANSACTIONS_DB_FILE);
    printf("==========================================\n");

    int fd = open(TRANSACTIONS_DB_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Could not open transactions.db");
        return;
    }

    txn_rec_t tx;
    int count = 1;
    while (read(fd, &tx, sizeof(txn_rec_t)) == sizeof(txn_rec_t)) {
        printf("\n--- Transaction Record %d ---\n", count++);
        printf("  Txn ID:       %llu\n", (unsigned long long)tx.txn_id);
        printf("  From Acct:    %u\n", tx.from_account);
        printf("  To Acct:      %u\n", tx.to_account);
        printf("  Amount:       %.2f\n", tx.amount);
        printf("  Narration:    %s\n", tx.narration);
        print_timestamp(tx.timestamp, "  Timestamp");
    }
    close(fd);
}

void print_loans() {
    printf("\n==========================================\n");
    printf("  DUMPING LOANS (from %s)\n", LOANS_DB_FILE);
    printf("==========================================\n");

    int fd = open(LOANS_DB_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Could not open loans.db");
        return;
    }

    loan_rec_t loan;
    int count = 1;
    while (read(fd, &loan, sizeof(loan_rec_t)) == sizeof(loan_rec_t)) {
        printf("\n--- Loan Record %d ---\n", count++);
        printf("  Loan ID:      %llu\n", (unsigned long long)loan.loan_id);
        printf("  User ID:      %u\n", loan.user_id);
        printf("  Amount:       %.2f\n", loan.amount);
        printf("  Status:       %s (%d)\n", get_loan_status_str(loan.status), loan.status);
        printf("  Assigned To:  %u\n", loan.assigned_to);
        printf("  Remarks:      %s\n", loan.remarks);
        print_timestamp(loan.applied_at, "  Applied At");
        print_timestamp(loan.processed_at, "  Processed At");
    }
    close(fd);
}

void print_feedback() {
    printf("\n==========================================\n");
    printf("  DUMPING FEEDBACK (from %s)\n", FEEDBACK_DB_FILE);
    printf("==========================================\n");

    int fd = open(FEEDBACK_DB_FILE, O_RDONLY);
    if (fd < 0) {
        perror("Could not open feedback.db");
        return;
    }

    feedback_rec_t fb;
    int count = 1;
    while (read(fd, &fb, sizeof(feedback_rec_t)) == sizeof(feedback_rec_t)) {
        printf("\n--- Feedback Record %d ---\n", count++);
        printf("  Feedback ID:  %llu\n", (unsigned long long)fb.fb_id);
        printf("  User ID:      %u\n", fb.user_id);
        printf("  Reviewed:     %s\n", fb.reviewed ? "YES" : "NO");
        printf("  Message:      %s\n", fb.message);
        printf("  Action Taken: %s\n", fb.action_taken);
        print_timestamp(fb.submitted_at, "  Submitted At");
    }
    close(fd);
}


/* --- Main Function --- */

int main() {
    printf("--- [Database Inspector Utility] ---\n");

    // Make sure the DB directory exists first
    if (access(DB_DIR, F_OK) == -1) {
        fprintf(stderr, "Error: DB directory '%s' not found.\n", DB_DIR);
        fprintf(stderr, "Are you running this from your project's root directory?\n");
        return EXIT_FAILURE;
    }

    print_users();
    print_accounts();
    print_transactions();
    print_loans();
    print_feedback();

    printf("\n--- [Inspection Complete] ---\n");
    return EXIT_SUCCESS;
}