/*
 * src/server.c
 * Core multithreaded TCP server for the Banking Management System.
 * Compatible with macOS (uses pthread + fcntl locks).
 */

#include "include/server.h"
#include "include/utils.h"

/* ---------- Forward declarations ---------- */
static int handle_request(int client_fd, const request_t *req, response_t *resp);

/* ---------- Utility: send/receive helpers ---------- */
ssize_t send_response(int fd, const response_t *resp) {
    if (!resp) return -1;
    size_t total = sizeof(response_t);
    return write_all(fd, resp, total);
}

ssize_t recv_request(int fd, request_t *req) {
    if (!req) return -1;
    size_t total = sizeof(request_t);
    ssize_t n = read_all(fd, req, total);
    if (n <= 0) return n;
    req->op[sizeof(req->op) - 1] = '\0';
    req->payload[sizeof(req->payload) - 1] = '\0';
    return n;
}

/* ---------- server_init ---------- */
int server_init(server_ctx_t *ctx, int port) {
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));

    ctx->port = (port > 0) ? port : DEFAULT_PORT;
    ctx->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->listen_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(ctx->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(ctx->port);

    if (bind(ctx->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(ctx->listen_fd);
        return -1;
    }

    if (listen(ctx->listen_fd, BACKLOG) < 0) {
        perror("listen");
        close(ctx->listen_fd);
        return -1;
    }

    pthread_mutex_init(&ctx->db_lock, NULL);
    ctx->running = 1;
    log_info("Server initialized on port %d", ctx->port);
    ensure_db_dir_exists();
    return 0;
}

/* ---------- server_start ---------- */
int server_start(server_ctx_t *ctx) {
    if (!ctx || ctx->listen_fd <= 0) return -1;
    log_info("Server running... Waiting for connections.");

    while (ctx->running) {
        client_ctx_t *cctx = (client_ctx_t *)malloc(sizeof(client_ctx_t));
        if (!cctx) continue;
        socklen_t len = sizeof(cctx->client_addr);

        cctx->client_fd = accept(ctx->listen_fd, 
                                 (struct sockaddr *)&cctx->client_addr, 
                                 &len);
        if (cctx->client_fd < 0) {
            if (errno == EINTR) {
                free(cctx);
                continue;
            }
            perror("accept");
            free(cctx);
            continue;
        }

        log_info("New client connected: %s:%d",
                 inet_ntoa(cctx->client_addr.sin_addr),
                 ntohs(cctx->client_addr.sin_port));

        pthread_t tid;
        pthread_create(&tid, NULL, client_thread_main, (void *)cctx);
        pthread_detach(tid);
    }

    return 0;
}

/* ---------- server_stop ---------- */
void server_stop(server_ctx_t *ctx) {
    if (!ctx) return;
    ctx->running = 0;
    close(ctx->listen_fd);
    pthread_mutex_destroy(&ctx->db_lock);
    log_info("Server stopped.");
}

/* ---------- client_thread_main ---------- */
void *client_thread_main(void *arg) {
    client_ctx_t *cctx = (client_ctx_t *)arg;
    int fd = cctx->client_fd;
    free(cctx);  /* no longer needed after extracting fd */

    request_t req;
    response_t resp;

    while (1) {
        ssize_t n = recv_request(fd, &req);
        if (n <= 0) {
            log_info("Client disconnected (fd=%d)", fd);
            break;
        }

        memset(&resp, 0, sizeof(resp));

        int ret = handle_request(fd, &req, &resp);
        if (ret < 0) {
            resp.status_code = 500;
            snprintf(resp.message, sizeof(resp.message), "Server error.");
        }

        send_response(fd, &resp);
    }

    close(fd);
    pthread_exit(NULL);
    return NULL;
}

/* ---------- handle_request ---------- */
/*
 * For now, this just demonstrates a few basic operations:
 *   LOGIN, PING, LOGOUT
 * Later, you’ll connect this with user_module.c, etc.
 */
static int handle_request(int client_fd, const request_t *req, response_t *resp) {
    if (!req || !resp) return -1;

    log_info("[fd=%d] Operation: %s", client_fd, req->op);

    if (strcasecmp(req->op, "PING") == 0) {
        resp->status_code = 0;
        snprintf(resp->message, sizeof(resp->message), "PONG");
        return 0;
    }

    if (strcasecmp(req->op, "LOGIN") == 0) {
        /* For now just simulate success */
        resp->status_code = 0;
        snprintf(resp->message, sizeof(resp->message), "Login successful (demo)");
        return 0;
    }

    if (strcasecmp(req->op, "LOGOUT") == 0) {
        resp->status_code = 0;
        snprintf(resp->message, sizeof(resp->message), "Logout successful (demo)");
        return 0;
    }

    resp->status_code = 400;
    snprintf(resp->message, sizeof(resp->message),
             "Unknown operation: %s", req->op);
    return 0;
}

/* ---------- main (optional server entrypoint) ---------- */
#ifdef BUILD_STANDALONE_SERVER
int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    server_ctx_t ctx;
    if (server_init(&ctx, port) == 0) {
        server_start(&ctx);
        server_stop(&ctx);
    } else {
        fprintf(stderr, "Failed to initialize server\n");
    }
    return 0;
}
#endif
