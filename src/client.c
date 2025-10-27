/*
 * src/client.c
 * Simple interactive client for the Banking Management System
 * Compatible with macOS (POSIX sockets)
 */

#include "include/client.h"
#include "include/utils.h"

/* ---------- client_connect ---------- */
int client_connect(client_app_ctx_t *ctx, const char *server_ip, int port) {
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));

    ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(ctx->sockfd);
        return -1;
    }

    if (connect(ctx->sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        close(ctx->sockfd);
        return -1;
    }

    strncpy(ctx->server_ip, server_ip, sizeof(ctx->server_ip) - 1);
    ctx->server_port = port;
    log_info("Connected to server %s:%d", ctx->server_ip, ctx->server_port);
    return 0;
}

/* ---------- client_disconnect ---------- */
void client_disconnect(client_app_ctx_t *ctx) {
    if (!ctx) return;
    close(ctx->sockfd);
    ctx->sockfd = -1;
    log_info("Disconnected from server.");
}

/* ---------- client_send_request ---------- */
int client_send_request(client_app_ctx_t *ctx, const request_t *req, response_t *resp) {
    if (!ctx || ctx->sockfd < 0) return -1;

    if (write_all(ctx->sockfd, req, sizeof(*req)) != sizeof(*req)) {
        perror("write_all");
        return -1;
    }

    if (read_all(ctx->sockfd, resp, sizeof(*resp)) != sizeof(*resp)) {
        perror("read_all");
        return -1;
    }

    return 0;
}

/* ---------- Simple demo operations ---------- */
int client_login(client_app_ctx_t *ctx, const char *username, const char *password) {
    request_t req;
    response_t resp;
    memset(&req, 0, sizeof(req));
    snprintf(req.op, sizeof(req.op), "LOGIN");
    snprintf(req.payload, sizeof(req.payload), "%s|%s", username, password);

    if (client_send_request(ctx, &req, &resp) == 0) {
        printf("Server: %s\n", resp.message);
        if (resp.status_code == 0)
            ctx->logged_in = 1;
        return resp.status_code;
    }
    return -1;
}

int client_logout(client_app_ctx_t *ctx) {
    request_t req;
    response_t resp;
    memset(&req, 0, sizeof(req));
    snprintf(req.op, sizeof(req.op), "LOGOUT");

    if (client_send_request(ctx, &req, &resp) == 0) {
        printf("Server: %s\n", resp.message);
        ctx->logged_in = 0;
        return resp.status_code;
    }
    return -1;
}

int client_ping(client_app_ctx_t *ctx) {
    request_t req;
    response_t resp;
    memset(&req, 0, sizeof(req));
    snprintf(req.op, sizeof(req.op), "PING");

    if (client_send_request(ctx, &req, &resp) == 0) {
        printf("Server: %s\n", resp.message);
        return resp.status_code;
    }
    return -1;
}

/* ---------- client_interactive_loop ---------- */
void client_interactive_loop(client_app_ctx_t *ctx) {
    while (1) {
        printf("\n=== Banking Management Client ===\n");
        printf("1. Ping Server\n");
        printf("2. Login\n");
        printf("3. Logout\n");
        printf("4. Exit\n");
        printf("Enter choice: ");

        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // flush
            continue;
        }

        if (choice == 1) {
            client_ping(ctx);
        } else if (choice == 2) {
            char uname[64], pass[64];
            printf("Username: ");
            scanf("%s", uname);
            printf("Password: ");
            scanf("%s", pass);
            client_login(ctx, uname, pass);
        } else if (choice == 3) {
            client_logout(ctx);
        } else if (choice == 4) {
            printf("Exiting client.\n");
            break;
        } else {
            printf("Invalid option.\n");
        }
    }
}

/* ---------- main (standalone client) ---------- */
#ifdef BUILD_STANDALONE_CLIENT
int main(int argc, char *argv[]) {
    const char *ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : DEFAULT_PORT;

    client_ctx_t ctx;
    if (client_connect(&ctx, ip, port) == 0) {
        client_interactive_loop(&ctx);
        client_disconnect(&ctx);
    } else {
        fprintf(stderr, "Failed to connect to server\n");
    }
    return 0;
}
#endif
