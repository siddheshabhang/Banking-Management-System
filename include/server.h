#ifndef SERVER_H
#define SERVER_H

/* Standard headers often needed by server-side modules */
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

/*
 * Configuration constants
 */
#define DEFAULT_PORT 9090
#define MAX_CLIENTS 100
#define BACKLOG 10
#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 128
#define MAX_NAME_LEN 128
#define MAX_ADDR_LEN 256
#define MAX_ROLE_STR 32
#define MAX_MSG_LEN 1024
#define DB_DIR "./db"             /* directory for persistence files */
#define USERS_DB_FILE DB_DIR"/users.db"
#define ACCOUNTS_DB_FILE DB_DIR"/accounts.db"
#define TRANSACTIONS_DB_FILE DB_DIR"/transactions.db"
#define LOANS_DB_FILE DB_DIR"/loans.db"
#define FEEDBACK_DB_FILE DB_DIR"/feedback.db"

/*
 * Roles enumerations
 */
typedef enum {
    ROLE_CUSTOMER = 0,
    ROLE_EMPLOYEE,
    ROLE_MANAGER,
    ROLE_ADMIN
} role_t;

/*
 * Generic status flags
 */
typedef enum {
    STATUS_INACTIVE = 0,
    STATUS_ACTIVE = 1
} status_t;

/*
 * User structure (persistent)
 * Keep primitive types and fixed-size strings for simple binary storage.
 */
typedef struct {
    uint32_t user_id;                         /* unique user id */
    char username[MAX_USERNAME_LEN];          /* login username (unique) */
    char password_hash[MAX_PASSWORD_LEN];     /* hashed password (store as hex/text) */
    role_t role;                              /* one of role_t */
    char name[MAX_NAME_LEN];                  /* full name */
    uint8_t age;
    char address[MAX_ADDR_LEN];
    status_t active;                          /* active/deactivated */
    time_t created_at;
} user_rec_t;

/*
 * Account structure (each customer has one account for simplicity)
 */
typedef struct {
    uint32_t account_id;      /* equal to user_id for simplicity or separate id */
    uint32_t user_id;         /* owner user id */
    double balance;
    status_t active;          /* account activated/deactivated */
    time_t created_at;
} account_rec_t;

/*
 * Transaction record
 */
typedef struct {
    uint64_t txn_id;
    uint32_t from_account;    /* 0 for deposit */
    uint32_t to_account;      /* 0 for withdrawal */
    double amount;
    time_t timestamp;
    char narration[128];
} txn_rec_t;

/*
 * Loan record
 */
typedef enum { LOAN_PENDING=0, LOAN_ASSIGNED, LOAN_APPROVED, LOAN_REJECTED } loan_status_t;
typedef struct {
    uint64_t loan_id;
    uint32_t user_id;
    double amount;
    loan_status_t status;
    uint32_t assigned_to;     /* employee id, 0 if not assigned */
    time_t applied_at;
    time_t processed_at;      /* when approved/rejected */
    char remarks[256];
} loan_rec_t;

/*
 * Feedback record
 */
typedef struct {
    uint64_t fb_id;
    uint32_t user_id;
    char message[512];
    int reviewed;             /* 0 = not reviewed, 1 = reviewed */
    char action_taken[256];
    time_t submitted_at;
} feedback_rec_t;

/*
 * Client handler context passed to thread function
 */
typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} client_ctx_t;

/*
 * Server runtime context
 */
typedef struct {
    int listen_fd;
    int port;
    pthread_t accept_thread;
    pthread_mutex_t db_lock;  /* coarse-grained mutex (you'll refine with file locks) */
    volatile int running;
} server_ctx_t;

/*
 * Request / Response primitives between client and server.
 * For now keep them simple string-based messages (JSON later if desired).
 */
typedef struct {
    char op[64];             /* operation code, e.g., "LOGIN", "VIEW_BALANCE" */
    char payload[MAX_MSG_LEN]; /* operation-specific payload */
} request_t;

typedef struct {
    int status_code;         /* 0 = success, non-zero = error */
    char message[MAX_MSG_LEN];
} response_t;

/*
 * --- Server function prototypes (implement in src/server.c) ---
 */

/* Initialize server context and create listening socket */
int server_init(server_ctx_t *ctx, int port);

/* Start the accept loop (spawns accept thread or runs loop) */
int server_start(server_ctx_t *ctx);

/* Stop the server and cleanup */
void server_stop(server_ctx_t *ctx);

/* Thread routine to handle a single connected client (implement logic to parse requests) */
void *client_thread_main(void *arg);

/* Utility to send a response_t over a socket (handles partial writes) */
ssize_t send_response(int fd, const response_t *resp);

/* Utility to receive a request_t from a socket (handles partial reads) */
ssize_t recv_request(int fd, request_t *req);

/* Helper to create DB directory if missing */
int ensure_db_dir_exists(void);

#endif /* SERVER_H */