#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "server.h" /* reuse request_t / response_t definitions */

/*
 * Client-side configuration and prototypes
 */
typedef struct {
    int sockfd;
    char server_ip[64];
    int server_port;
    int logged_in;           /* bool */
    uint32_t user_id;
    role_t role;
    char username[MAX_USERNAME_LEN];
} client_app_ctx_t;

/* Connect to server, returns 0 on success */
int client_connect(client_app_ctx_t *ctx, const char *server_ip, int port);

/* Disconnect client cleanly */
void client_disconnect(client_app_ctx_t *ctx);

/* Send a request and wait for a response (blocking) */
int client_send_request(client_app_ctx_t *ctx, const request_t *req, response_t *resp);

/* High-level client actions (call these from interactive menu) */
int client_login(client_app_ctx_t *ctx, const char *username, const char *password);
int client_change_password(client_app_ctx_t *ctx, const char *oldpass, const char *newpass);
int client_logout(client_app_ctx_t *ctx);

/* Convenience functions to format common requests */
int client_view_balance(client_app_ctx_t *ctx);
int client_deposit(client_app_ctx_t *ctx, double amount);
int client_withdraw(client_app_ctx_t *ctx, double amount);
int client_transfer(client_app_ctx_t *ctx, uint32_t to_account, double amount);
int client_apply_loan(client_app_ctx_t *ctx, double amount, const char *remarks);

/* Interactive client loop (implements menu & IO) */
void client_interactive_loop(client_app_ctx_t *ctx);

#endif /* CLIENT_H */
