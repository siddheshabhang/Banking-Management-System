#include "manager_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h> 
#include <fcntl.h> 

/* --- MANAGER MODULE LOGIC (Oversight/Approvals) --- */

// Modifier for set_account_status (Atomic update logic)
int set_status_modifier(account_rec_t *acc, void *data) {
    int new_status = *(int*)data;
    acc->active = (new_status == 1) ? STATUS_ACTIVE : STATUS_INACTIVE;
    return 1; // Always succeed
}

// set_account_status (Atomic change of account active status)
int set_account_status(uint32_t custId, int status, char *resp_msg, size_t resp_sz) {
    if (atomic_update_account(custId, set_status_modifier, &status)) {
         snprintf(resp_msg, resp_sz, "Account %u Status Updated to: %s", custId, (status == 1) ? "ACTIVE" : "INACTIVE");
         return 1;
    }

    snprintf(resp_msg, resp_sz, "Account Status Update Failed (Account not found)");
    return 0;
}


/* --- Modifier for assign_loan_to_employee (Atomic Loan Update Logic) --- */
typedef struct {
    uint32_t empId;
    char *resp_msg;
    size_t resp_sz;
} assign_loan_data;

int assign_loan_modifier(loan_rec_t *loan, void *data) {
    assign_loan_data *d = (assign_loan_data*)data;

    if (loan->status != LOAN_PENDING) {
        snprintf(d->resp_msg, d->resp_sz, "Loan is not pending, cannot assign."); 
        return 0; 
    }
    
    // Consistency Check: Validate employee role/existence
    user_rec_t emp;
    if(!read_user(d->empId, &emp) || emp.role != ROLE_EMPLOYEE) {
        snprintf(d->resp_msg, d->resp_sz, "Employee ID %u not found or is not an employee.", d->empId); 
        return 0; // Abort modification
    }

    // Modify loan record
    loan->assigned_to = d->empId;
    loan->status = LOAN_ASSIGNED; 
    
    snprintf(d->resp_msg, d->resp_sz, "Loan %llu Assigned to Employee %u", (unsigned long long)loan->loan_id, d->empId);
    return 1; 
}

// assign_loan_to_employee (Wrapper for atomic loan update)
int assign_loan_to_employee(uint32_t loanId, uint32_t empId, char *resp_msg, size_t resp_sz) {
    assign_loan_data data = {empId, resp_msg, resp_sz};
    
    if (atomic_update_loan(loanId, assign_loan_modifier, &data)) {
        return 1; 
    }
    
    if (resp_msg[0] == '\0') {
        snprintf(resp_msg, resp_sz, "Loan Assignment Failed (Loan not found or concurrency error)");
    }
    return 0;
}

// view_non_assigned_loans (Read-only list)
int view_non_assigned_loans(char *resp_msg, size_t resp_sz) {
    int fd = open(LOANS_DB_FILE,O_RDONLY);
    if(fd<0) { snprintf(resp_msg,resp_sz,"No loans file found"); return 0; }
    
    lock_file(fd);
    loan_rec_t loan;
    char tmp[256]; 
    resp_msg[0]='\0';
    int found = 0;

    strncat(resp_msg, "--- Non-Assigned (Pending) Loans ---\n", resp_sz - 1);
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
    
    if(!found) {
        snprintf(resp_msg, resp_sz, "No non-assigned loans found.");
    }
    return 1;
}

// review_feedbacks (CRITICAL: Batch Update, uses full-file lock)
int review_feedbacks(char *resp_msg, size_t resp_sz) {
    int fd = open(FEEDBACK_DB_FILE,O_RDWR);
    if(fd<0) { snprintf(resp_msg,resp_sz,"No feedback file found"); return 0; }
    
    lock_file(fd);
    feedback_rec_t fb; 
    off_t pos=0;
    int reviewed_count = 0;
    
    resp_msg[0] = '\0';
    strncat(resp_msg, "--- Unreviewed Feedback ---\n", resp_sz - 1);

    while(read(fd,&fb,sizeof(feedback_rec_t))==sizeof(feedback_rec_t)) {
        if(fb.reviewed == 0) {
            
            char tmp[600];
            snprintf(tmp, sizeof(tmp), "ID: %llu, User: %u, Msg: \"%.100s\"\n",
                (unsigned long long)fb.fb_id, fb.user_id, fb.message);
            strncat(resp_msg, tmp, resp_sz - strlen(resp_msg) - 1);

            fb.reviewed=1;
            lseek(fd, pos, SEEK_SET); 
            write(fd, &fb, sizeof(feedback_rec_t));
            lseek(fd, pos + sizeof(feedback_rec_t), SEEK_SET); 
            
            reviewed_count++;
        }
        pos += sizeof(feedback_rec_t);
    }
    
    unlock_file(fd); 
    close(fd);
    
    if (reviewed_count == 0) {
        snprintf(resp_msg,resp_sz,"No new feedback found to review.");
    } else {
        char tmp[100];
        snprintf(tmp, sizeof(tmp), "\n%d feedback(s) marked as reviewed.", reviewed_count);
        strncat(resp_msg, tmp, resp_sz - strlen(resp_msg) - 1);
    }
    return 1;
}