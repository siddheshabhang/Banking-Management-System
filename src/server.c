#include "server.h"
#include "customer_module.h"
#include "employee_module.h"
#include "manager_module.h"
#include "admin_module.h"
#include "utils.h"

#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>

#define BACKLOG 10

// --- Utility functions ---
int ensure_db_dir_exists(void) {
    struct stat st = {0};
    if(stat(DB_DIR,&st)==-1) {
        if(mkdir(DB_DIR,0700)!=0) {
            perror("Failed to create DB directory");
            return -1;
        }
    }
    return 0;
}

// Send response
ssize_t send_response(int fd, const response_t *resp) {
    return write(fd, resp, sizeof(response_t));
}

// Receive request
ssize_t recv_request(int fd, request_t *req) {
    return read(fd, req, sizeof(request_t));
}

// --- Client thread ---
void *client_thread_main(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int fd = ctx->client_fd;
    request_t req;
    response_t resp;

    while(recv_request(fd,&req) > 0) {
        memset(&resp,0,sizeof(resp));
        resp.status_code = 0;

        char *op = req.op;
        char *payload = req.payload;

        // --- Customer Commands ---
        if(strcmp(op,"VIEW_BALANCE")==0) {
            uint32_t userId;
            sscanf(payload,"%u",&userId);
            view_balance(userId, resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"DEPOSIT")==0) {
            uint32_t userId; double amount;
            sscanf(payload,"%u %lf",&userId,&amount);
            deposit_money(userId,amount,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"WITHDRAW")==0) {
            uint32_t userId; double amount;
            sscanf(payload,"%u %lf",&userId,&amount);
            withdraw_money(userId,amount,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"TRANSFER")==0) {
            uint32_t fromId,toId; double amount;
            sscanf(payload,"%u %u %lf",&fromId,&toId,&amount);
            transfer_funds(fromId,toId,amount,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"APPLY_LOAN")==0) {
            uint32_t userId; double amount;
            sscanf(payload,"%u %lf",&userId,&amount);
            apply_loan(userId,amount,resp.message,sizeof(resp.message));
        }

        // --- Employee Commands ---
        else if(strcmp(op,"ADD_CUSTOMER")==0) {
            user_rec_t user; 
            account_rec_t acc;  // assume payload parsed properly
            char name[MAX_NAME_LEN], address[MAX_ADDR_LEN], username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN]; 
            int age;
            // We parse all 5 fields from the payload
            if (sscanf(payload,"%s %d %s %s %s", name, &age, address, username, password) == 5) {
                 // We populate the user struct with the data
                 strncpy(user.name, name, sizeof(user.name) - 1);
                 user.age = age;
                 strncpy(user.address, address, sizeof(user.address) - 1);
                 
                 // FIX: We now call the function with all 6 required arguments
                 add_new_customer(&user, &acc, username, password, resp.message, sizeof(resp.message));
            } else {
                 snprintf(resp.message,sizeof(resp.message),"ADD_CUSTOMER: Invalid payload format.");
                 resp.status_code = 1;
            }
        }
        else if(strcmp(op,"MODIFY_CUSTOMER")==0) {
            uint32_t userId; char name[128],address[256]; int age;
            sscanf(payload,"%u %s %d %s",&userId,name,&age,address);
            modify_customer(userId,name,age,address,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"APPROVE_REJECT_LOAN")==0) {
            uint64_t loanId; uint32_t empId; char action[32];
            sscanf(payload,"%llu %s %u",(unsigned long long *)&loanId, action, &empId);
            approve_reject_loan(loanId,action,empId,resp.message,sizeof(resp.message));
        }

        // --- Manager Commands ---
        else if(strcmp(op,"SET_ACCOUNT_STATUS")==0) {
            uint32_t custId,status;
            sscanf(payload,"%u %u",&custId,&status);
            set_account_status(custId,status,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"ASSIGN_LOAN")==0) {
            uint32_t loanId,empId;
            sscanf(payload,"%u %u",&loanId,&empId);
            assign_loan_to_employee(loanId,empId,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"REVIEW_FEEDBACK")==0) {
            review_feedbacks(resp.message,sizeof(resp.message));
        }

        // --- Admin Commands ---
        else if(strcmp(op,"ADD_EMPLOYEE")==0) {
            char name[MAX_NAME_LEN],address[MAX_ADDR_LEN],role[MAX_ROLE_STR],username[MAX_USERNAME_LEN],password[MAX_PASSWORD_LEN];
            int age;
            
            // FIX: This sscanf correctly parses all 6 fields, including 'age'
            if(sscanf(payload,"%s %d %s %s %s %s",name,&age,address,role,username,password) == 6) {
                add_employee(name,age,address,role,username,password,resp.message,sizeof(resp.message));
            } else {
                snprintf(resp.message,sizeof(resp.message),"ADD_EMPLOYEE: Invalid payload format.");
                resp.status_code = 1;
            }
        }
        else if(strcmp(op,"MODIFY_USER")==0) {
            uint32_t userId; char name[128],address[256]; int age;
            sscanf(payload,"%u %s %d %s",&userId,name,&age,address);
            modify_user(userId,name,age,address,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"CHANGE_ROLE")==0) {
            uint32_t userId; char role[32];
            sscanf(payload,"%u %s",&userId,role);
            change_user_role(userId,role,resp.message,sizeof(resp.message));
        }

        // --- Login / Logout / Password ---
        else if(strcmp(op,"LOGIN")==0) {
            char username[64],password[64];
            sscanf(payload,"%s %s",username,password);
            int userId; char role[32];
            if(login_user(username,password,&userId,role,sizeof(role)))
                snprintf(resp.message,sizeof(resp.message),"SUCCESS %d %s",userId,role);
            else snprintf(resp.message,sizeof(resp.message),"FAILURE Invalid Credentials");
        }
        else if(strcmp(op,"CHANGE_PASSWORD")==0) {
            uint32_t userId; char newpass[64];
            sscanf(payload,"%u %s",&userId,newpass);
            change_password(userId,newpass,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"LOGOUT")==0) {
            snprintf(resp.message,sizeof(resp.message),"Logged out successfully");
        }
        else snprintf(resp.message,sizeof(resp.message),"Unknown command");

        send_response(fd,&resp);
    }

    close(fd);
    free(ctx);
    return NULL;
}

// --- Server initialization ---
int server_init(server_ctx_t *ctx, int port) {
    if(ensure_db_dir_exists()!=0) return -1;
    ctx->port = port;
    ctx->listen_fd = socket(AF_INET, SOCK_STREAM,0);
    if(ctx->listen_fd<0) { perror("socket"); return -1; }
    ctx->running=1;
    pthread_mutex_init(&ctx->db_lock,NULL);
    return 0;
}

// --- Server start ---
int server_start(server_ctx_t *ctx) {
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(ctx->port);
    addr.sin_addr.s_addr=INADDR_ANY;

    if(bind(ctx->listen_fd,(struct sockaddr*)&addr,sizeof(addr))<0) {
        perror("bind"); return -1;
    }

    if(listen(ctx->listen_fd,BACKLOG)<0) { perror("listen"); return -1; }

    printf("Server listening on port %d...\n",ctx->port);

    while(ctx->running) {
        client_ctx_t *cctx = malloc(sizeof(client_ctx_t));
        socklen_t len = sizeof(cctx->client_addr);
        cctx->client_fd = accept(ctx->listen_fd,(struct sockaddr*)&cctx->client_addr,&len);
        if(cctx->client_fd<0) { free(cctx); continue; }

        pthread_t tid;
        pthread_create(&tid,NULL,client_thread_main,cctx);
        pthread_detach(tid);
    }
    return 0;
}

// --- Stop server ---
void server_stop(server_ctx_t *ctx) {
    ctx->running=0;
    close(ctx->listen_fd);
    pthread_mutex_destroy(&ctx->db_lock);
}

/*
 * Add this code to the BOTTOM of src/server.c
 */
#include <signal.h> // For SIGINT

// Global context for the signal handler
server_ctx_t g_server_ctx;

void sigint_handler(int sig) {
    printf("\nCaught SIGINT (Ctrl+C), shutting down server...\n");
    server_stop(&g_server_ctx);
    // The server_start loop will exit when g_server_ctx.running is 0
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    // Set up the graceful shutdown handler
    signal(SIGINT, sigint_handler);
    
    if (server_init(&g_server_ctx, port) != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }
    
    // This is the blocking call that runs the server
    printf("Server setup complete. Starting accept loop...\n");
    server_start(&g_server_ctx);
    
    // Cleanup (will be reached after server_stop is called)
    printf("Server main loop exited. Goodbye.\n");
    return 0;
}