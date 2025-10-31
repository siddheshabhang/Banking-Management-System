#ifndef SERVER_H 
#define SERVER_H 
#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>

#define DEFAULT_PORT 9090 
#define MAX_CLIENTS 100
#define BACKLOG 10
#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 128
#define MAX_ADDR_LEN 256
#define MAX_ROLE_STR 32
#define MAX_MSG_LEN 1024
#define MAX_FNAME_LEN 64
#define MAX_LNAME_LEN 64
#define MAX_EMAIL_LEN 64
#define MAX_PHONE_LEN 16
#define DB_DIR "./db"         
#define USERS_DB_FILE DB_DIR"/users.db"
#define ACCOUNTS_DB_FILE DB_DIR"/accounts.db"
#define TRANSACTIONS_DB_FILE DB_DIR"/transactions.db"
#define LOANS_DB_FILE DB_DIR"/loans.db"
#define FEEDBACK_DB_FILE DB_DIR"/feedback.db"

typedef enum {  
    ROLE_CUSTOMER = 0,
    ROLE_EMPLOYEE,
    ROLE_MANAGER,
    ROLE_ADMIN
} role_t;
typedef enum {
    STATUS_INACTIVE = 0,
    STATUS_ACTIVE = 1
} status_t;
typedef struct {
    uint32_t user_id;                         
    char username[MAX_USERNAME_LEN];          
    char password_hash[MAX_PASSWORD_LEN];     
    role_t role;                              
    char first_name[MAX_FNAME_LEN];
    char last_name[MAX_LNAME_LEN];               
    uint8_t age;
    char address[MAX_ADDR_LEN];
    char email[MAX_EMAIL_LEN];
    char phone[MAX_PHONE_LEN];
    status_t active;                         
    time_t created_at;
} user_rec_t;
typedef struct {
    uint32_t account_id;      
    uint32_t user_id;         
    double balance;
    status_t active;          
} account_rec_t;
typedef struct {
    uint64_t txn_id;
    uint32_t from_account;    
    uint32_t to_account;      
    double amount;
    time_t timestamp;
    char narration[128];
} txn_rec_t;

typedef enum { LOAN_PENDING=0, LOAN_ASSIGNED, LOAN_APPROVED, LOAN_REJECTED } loan_status_t;
typedef struct {
    uint64_t loan_id;
    uint32_t user_id;
    double amount;
    loan_status_t status;
    uint32_t assigned_to;     
    time_t applied_at;
    time_t processed_at;      
    char remarks[256];
} loan_rec_t;
typedef struct {
    uint64_t fb_id;
    uint32_t user_id;
    char message[512];
    int reviewed;             
    char action_taken[256];
    time_t submitted_at;
} feedback_rec_t;
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_ctx_t;
typedef struct {
    int listen_fd;
    int port;
    pthread_t accept_thread;
    pthread_mutex_t db_lock;  
    volatile int running;
} server_ctx_t;
typedef struct {
    char op[64];             
    char payload[MAX_MSG_LEN]; 
} request_t;

typedef struct {
    int status_code;         
    char message[MAX_MSG_LEN];
} response_t;

int server_init(server_ctx_t *ctx, int port);
int server_start(server_ctx_t *ctx);
void server_stop(server_ctx_t *ctx);
void *client_thread_main(void *arg);
ssize_t send_response(int fd, const response_t *resp);
ssize_t recv_request(int fd, request_t *req);
int ensure_db_dir_exists(void);

#endif 