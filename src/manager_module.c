#include "manager_module.h"
#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h> // for lseek
#include <fcntl.h> // for O_RDONLY

int set_account_status(uint32_t custId, int status, char *resp_msg, size_t resp_sz) {
    account_rec_t acc;
    if(!read_account(custId, &acc)) {
        snprintf(resp_msg, resp_sz, "Account not found");
        return 0;
    }
    
    // Ensure status is either 0 or 1
    acc.active = (status == 1) ? STATUS_ACTIVE : STATUS_INACTIVE;
    
    if(write_account(&acc)) {
        snprintf(resp_msg, resp_sz, "Account %u Status Updated to: %s", custId, acc.active ? "ACTIVE" : "INACTIVE");
        return 1;
    }
    
    snprintf(resp_msg, resp_sz, "Account Status Update Failed (Write Error)");
    return 0;
}

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

int assign_loan_to_employee(uint32_t loanId, uint32_t empId, char *resp_msg, size_t resp_sz) {
    loan_rec_t loan;
    if(!read_loan(loanId,&loan)) { 
        snprintf(resp_msg,resp_sz,"Loan ID %u not found", loanId); 
        return 0; 
    }
    
    if (loan.status != LOAN_PENDING) {
        snprintf(resp_msg,resp_sz,"Loan is not pending, cannot assign."); 
        return 0;
    }
    
    // Check if empId is a valid employee
    user_rec_t emp;
    if(!read_user(empId, &emp) || emp.role != ROLE_EMPLOYEE) {
        snprintf(resp_msg,resp_sz,"Employee ID %u not found or is not an employee.", empId); 
        return 0;
    }

    loan.assigned_to = empId;
    loan.status = LOAN_ASSIGNED; // Update status
    
    if(write_loan(&loan)) {
        snprintf(resp_msg,resp_sz,"Loan %u Assigned to Employee %u", loanId, empId);
        return 1;
    }
    
    snprintf(resp_msg,resp_sz,"Loan Assignment Failed (Write Error)");
    return 0;
}

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
            
            // Append to response message
            char tmp[600];
            snprintf(tmp, sizeof(tmp), "ID: %llu, User: %u, Msg: \"%.100s\"\n",
                (unsigned long long)fb.fb_id, fb.user_id, fb.message);
            strncat(resp_msg, tmp, resp_sz - strlen(resp_msg) - 1);

            // Mark as reviewed and write back
            fb.reviewed=1;
            lseek(fd, pos, SEEK_SET); // Go back to the start of this record
            write(fd, &fb, sizeof(feedback_rec_t));
            lseek(fd, pos + sizeof(feedback_rec_t), SEEK_SET); // Move to next record
            
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