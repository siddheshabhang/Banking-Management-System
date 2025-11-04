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

// --- Global Context and Session Tracking ---

extern server_ctx_t g_server_ctx;
#define MAX_SESSIONS MAX_CLIENTS
// Tracks logged-in user IDs (0 means slot is empty)
// This array is protected by the db_lock mutex.
int active_sessions[MAX_SESSIONS]; 

/*
 * ensure_db_dir_exists
 * Makes sure the DB_DIR directory ("db/") exists.
 */
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

/*
 * send_response
 * Helper function to write a response_t struct to a socket.
 */
ssize_t send_response(int fd, const response_t *resp) {
    return write(fd, resp, sizeof(response_t));
}

/*
 * recv_request
 * Helper function to read a request_t struct from a socket.
 */
ssize_t recv_request(int fd, request_t *req) {
    return read(fd, req, sizeof(request_t));
}

/*
 * remove_active_session
 * Finds and removes a userId from the global active_sessions array.
 * This function is concurrency-safe as it's called within a mutex.
 */
void remove_active_session(int userId) {
    pthread_mutex_lock(&g_server_ctx.db_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (active_sessions[i] == userId) {
            active_sessions[i] = 0; 
            break;
        }
    }
    pthread_mutex_unlock(&g_server_ctx.db_lock);
}

// --- Main Client Thread ---

/*
 * client_thread_main
 * This is the main loop for each connected client.
 * It reads requests, routes them to the correct module, and sends back responses.
 */
void *client_thread_main(void *arg) {
    client_ctx_t *ctx = (client_ctx_t *)arg;
    int fd = ctx->client_fd;
    request_t req;
    response_t resp;
    int current_userId = 0; // Tracks the user ID for this session

    // The main request router loop
    while(recv_request(fd,&req) > 0) {
        memset(&resp,0,sizeof(resp));
        resp.status_code = 0;       // Default to success

        char *op = req.op;
        char *payload = req.payload;

        // --- SECTION: Authentication & Session Management ---

        if(strcmp(op,"LOGIN") == 0) {
            char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
            sscanf(payload,"%s %s",username,password);
            int userId; 
            char role[MAX_ROLE_STR];
            char name[MAX_FNAME_LEN]; 
            int login_result; 

            login_result = login_user(username, password, &userId, role, sizeof(role), name, sizeof(name)); 

            if (login_result == 2) {    // Account is deactivated
                snprintf(resp.message,sizeof(resp.message),"FAILURE! Account is deactivated. Please contact your bank.");
            }
            else if (login_result == 1) {      // Successful password and active status
                // This block prevents concurrent (double) logins from the same user.
                // It uses a mutex (db_lock) to protect the global 'active_sessions' array,
                // ensuring that checking and setting a session is an atomic operation.
                pthread_mutex_lock(&g_server_ctx.db_lock);
                int already_logged_in = 0;
                for (int i = 0; i < MAX_SESSIONS; i++) {
                    if (active_sessions[i] == userId) {
                        already_logged_in = 1;
                        break;
                    }
                }
                
                if (already_logged_in) {
                    pthread_mutex_unlock(&g_server_ctx.db_lock);
                    snprintf(resp.message,sizeof(resp.message),"FAILURE User is already logged in elsewhere.");
                } else {
                    // Add user to active sessions
                    for (int i = 0; i < MAX_SESSIONS; i++) {
                        if (active_sessions[i] == 0) {
                            active_sessions[i] = userId;
                            current_userId = userId; 
                            break;
                        }
                    }
                    pthread_mutex_unlock(&g_server_ctx.db_lock);
                    snprintf(resp.message, sizeof(resp.message), "SUCCESS %d %s|%s", userId, role, name);
                }
            }
            else {      // Invalid credentials
                snprintf(resp.message,sizeof(resp.message),"FAILURE Invalid Credentials");
            }
        }
        else if(strcmp(op,"LOGOUT")==0) {
            remove_active_session(current_userId);
            current_userId = 0;
            snprintf(resp.message,sizeof(resp.message),"Logged out successfully");
        }
        else if(strcmp(op,"CHANGE_PASSWORD")==0) {
            uint32_t userId; 
            char newpass[MAX_PASSWORD_LEN];
            sscanf(payload,"%u %s", &userId, newpass);
            change_password(userId, newpass, resp.message, sizeof(resp.message));
        }

        // --- SECTION: Customer Module Routes ---

        else if(strcmp(op,"VIEW_BALANCE")==0) {
            uint32_t userId;
            sscanf(payload,"%u",&userId);
            view_balance(userId, resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"DEPOSIT")==0) {
            uint32_t userId; 
            double amount;
            sscanf(payload,"%u %lf",&userId, &amount);
            deposit_money(userId, amount, resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"WITHDRAW")==0) {
            uint32_t userId; 
            double amount;
            sscanf(payload,"%u %lf",&userId,&amount);
            withdraw_money(userId, amount, resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"TRANSFER")==0) { 
            uint32_t fromId, toId; 
            double amount;
            if(sscanf(payload,"%u %u %lf",&fromId,&toId,&amount) == 3) {
                transfer_funds(fromId,toId,amount,resp.message,sizeof(resp.message));
            } else {
                snprintf(resp.message,sizeof(resp.message),"TRANSFER: Invalid payload format.");
                resp.status_code = 1;
            }
        }
        else if(strcmp(op,"APPLY_LOAN")==0) {
            uint32_t userId; 
            double amount;
            sscanf(payload,"%u %lf",&userId, &amount);
            apply_loan(userId, amount, resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"VIEW_LOAN")==0) { 
            uint32_t userId;
            sscanf(payload,"%u",&userId);
            view_loan_status(userId, resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"ADD_FEEDBACK")==0) { 
            uint32_t userId;
            char *msg_start;
            
            if (sscanf(payload, "%u", &userId) == 1) {
                msg_start = strchr(payload, ' ');
                if (msg_start != NULL) {
                    add_feedback(userId, msg_start + 1, resp.message, sizeof(resp.message));
                } else {
                    add_feedback(userId, "", resp.message, sizeof(resp.message)); 
                }
            } else {
                snprintf(resp.message,sizeof(resp.message),"ADD_FEEDBACK: Invalid payload format.");
                resp.status_code = 1;
            }
        }
        else if(strcmp(op,"VIEW_FEEDBACK")==0) { 
            uint32_t userId;
            sscanf(payload,"%u",&userId);
            view_feedback_status(userId, resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"VIEW_TRANSACTIONS")==0) { 
            uint32_t userId;
            sscanf(payload,"%u",&userId);
            view_transaction_history(userId, resp.message, sizeof(resp.message));
        }

        else if(strcmp(op,"VIEW_DETAILS")==0) { 
            uint32_t userId;
            sscanf(payload,"%u",&userId);
            view_personal_details(userId, resp.message, sizeof(resp.message));
        }

        // --- SECTION: Employee Module Routes ---

        else if(strcmp(op,"ADD_CUSTOMER")==0) {
            user_rec_t user; 
            account_rec_t acc;
            char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], address[MAX_ADDR_LEN], email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN];
            char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN]; 
            int age;
            if (sscanf(payload,"%s %s %d %s %s %s %s %s", 
                 fname, lname, &age, address, email, phone, username, password) == 8) {
                 
                 strncpy(user.first_name, fname, sizeof(user.first_name) - 1);
                 strncpy(user.last_name, lname, sizeof(user.last_name) - 1);
                 user.age = age;
                 strncpy(user.address, address, sizeof(user.address) - 1);
                 strncpy(user.email, email, sizeof(user.email) - 1);
                 strncpy(user.phone, phone, sizeof(user.phone) - 1);
                 add_new_customer(&user, &acc, username, password, resp.message, sizeof(resp.message));
            } else {
                 snprintf(resp.message,sizeof(resp.message),"ADD_CUSTOMER: Invalid payload format.");
                 resp.status_code = 1;
            }
        }
        else if(strcmp(op,"MODIFY_CUSTOMER")==0) {
            uint32_t userId; char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], address[MAX_ADDR_LEN], email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN]; int age;
            
            if (sscanf(payload,"%u %d %s %s %s %s %s",&userId,&age,fname,lname,address, email, phone) == 7) {
                modify_customer(userId,fname,lname,age,address,email,phone,resp.message,sizeof(resp.message));
            } else {
                snprintf(resp.message,sizeof(resp.message),"MODIFY_CUSTOMER: Invalid payload format.");
                resp.status_code = 1;
            }
        }
        else if(strcmp(op,"PROCESS_LOANS")==0) { 
            process_loans(resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"APPROVE_REJECT_LOAN")==0) {
            uint64_t loanId; uint32_t empId; char action[32];
            sscanf(payload,"%llu %s %u",(unsigned long long *)&loanId, action, &empId);
            approve_reject_loan(loanId,action,empId,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"VIEW_ASSIGNED_LOANS")==0) {
            uint32_t empId;
            sscanf(payload,"%u",&empId);
            view_assigned_loans(empId,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"VIEW_CUST_TRANSACTIONS")==0) { 
            uint32_t custId;
            sscanf(payload,"%u",&custId);
            view_customer_transactions(custId,resp.message,sizeof(resp.message));
        }

        // --- SECTION: Manager Module Routes ---

        else if(strcmp(op,"SET_ACCOUNT_STATUS")==0) {
            uint32_t custId,status;
            sscanf(payload,"%u %u",&custId,&status);
            set_account_status(custId,status,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"VIEW_NON_ASSIGNED_LOANS")==0) { 
            view_non_assigned_loans(resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"ASSIGN_LOAN")==0) {
            uint32_t loanId,empId;
            sscanf(payload,"%u %u",&loanId,&empId);
            assign_loan_to_employee(loanId,empId,resp.message,sizeof(resp.message));
        }
        else if(strcmp(op,"REVIEW_FEEDBACK")==0) {
            review_feedbacks(resp.message,sizeof(resp.message));
        }

        // --- SECTION: Admin Module Routes ---

        else if(strcmp(op,"ADD_EMPLOYEE")==0) {
            char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], address[MAX_ADDR_LEN],role[MAX_ROLE_STR];
            char email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN];
            char username[MAX_USERNAME_LEN],password[MAX_PASSWORD_LEN];
            int age;
            
            if(sscanf(payload,"%s %s %d %s %s %s %s %s %s",
                fname, lname, &age,address,role,email,phone,username,password) == 9) {
                add_employee(fname,lname,age,address,role,email,phone,username,password,resp.message,sizeof(resp.message));
            } else {
                snprintf(resp.message,sizeof(resp.message),"ADD_EMPLOYEE: Invalid payload format.");
                resp.status_code = 1;
            }
        }
        else if(strcmp(op,"MODIFY_USER")==0) {
            uint32_t userId; char fname[MAX_FNAME_LEN], lname[MAX_LNAME_LEN], address[MAX_ADDR_LEN], email[MAX_EMAIL_LEN], phone[MAX_PHONE_LEN]; int age;

            if (sscanf(payload,"%u %d %s %s %s %s %s",&userId,&age,fname,lname,address,email,phone) == 6) {
                modify_user(userId,fname,lname,age,address,email,phone,resp.message,sizeof(resp.message));
            } else {
                snprintf(resp.message,sizeof(resp.message),"MODIFY_USER: Invalid payload format.");
                resp.status_code = 1;
            }
        }
        else if(strcmp(op,"LIST_USERS")==0) {
            list_all_users(resp.message, sizeof(resp.message));
        }
        else if(strcmp(op,"CHANGE_ROLE")==0) {
            uint32_t userId; char role[32];
            sscanf(payload,"%u %s",&userId,role);
            change_user_role(userId,role,resp.message,sizeof(resp.message));
        }
        else snprintf(resp.message,sizeof(resp.message),"Unknown command");

        send_response(fd,&resp);
    }

    // --- Cleanup on thread exit (client disconnect) ---
    if (current_userId != 0) {
        // If the user was logged in, ensure their session is cleared from the global tracker.
        remove_active_session(current_userId);
    }
    close(fd);
    free(ctx);
    return NULL;
}

// --- Server Lifecycle Functions ---

/*
 * server_init
 * Initializes the server context, creates the listen socket,
 * and initializes the session tracker and mutex.
 */
int server_init(server_ctx_t *ctx, int port) {
    if(ensure_db_dir_exists() != 0) 
        return -1;
    ctx->port = port;
    ctx->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(ctx->listen_fd<0) { 
        perror("socket"); 
        return -1; 
    }
    ctx->running=1;
    memset(active_sessions, 0, sizeof(active_sessions));
    pthread_mutex_init(&ctx->db_lock,NULL);
    return 0;
}

/*
 * server_start
 * Binds, listens, and enters the main accept() loop.
 * Spawns a new detached thread for each new client.
 */
int server_start(server_ctx_t *ctx) {
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(ctx->port);
    addr.sin_addr.s_addr=INADDR_ANY;

    int opt = 1;
    if (setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        return -1;
    }

    if(bind(ctx->listen_fd,(struct sockaddr*)&addr,sizeof(addr))<0) {
        perror("bind"); return -1;
    }

    if(listen(ctx->listen_fd,BACKLOG)<0) { 
        perror("listen"); return -1; 
    }

    printf("Server listening on port %d...\n",ctx->port);

    while(ctx->running) {
        client_ctx_t *cctx = malloc(sizeof(client_ctx_t));
        if (cctx == NULL) {
            fprintf(stderr, "Failed to allocate memory for client context.\n");
            continue;
        }
        socklen_t len = sizeof(cctx->client_addr);
        cctx->client_fd = accept(ctx->listen_fd,(struct sockaddr*)&cctx->client_addr,&len);
        if(cctx->client_fd < 0) { 
            free(cctx); 
            continue; 
        }

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread_main, cctx) != 0) {
            perror("pthread_create");
            close(cctx->client_fd);
            free(cctx);
        } else {
            pthread_detach(tid);
        }
    }
    return 0;
}

/*
 * server_stop
 * Shuts down the server gracefully.
 */
void server_stop(server_ctx_t *ctx) {
    ctx->running=0;
    close(ctx->listen_fd);
    pthread_mutex_destroy(&ctx->db_lock);
}

// --- Main Function ---

server_ctx_t g_server_ctx;

/*
 * sigint_handler
 * Catches Ctrl+C to allow for a graceful shutdown.
 */
void sigint_handler(int sig) {
    printf("\nCaught SIGINT (Ctrl+C), shutting down server...\n");
    server_stop(&g_server_ctx);
}

/*
 * main
 * Entry point for the server executable.
 */
int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    signal(SIGINT, sigint_handler);
    
    if (server_init(&g_server_ctx, port) != 0) {
        fprintf(stderr, "Failed to initialize server\n");
        return 1;
    }

    printf("Server setup complete. Starting accept loop...\n");
    server_start(&g_server_ctx);
    
    printf("Server main loop exited. Goodbye.\n");
    return 0;
}