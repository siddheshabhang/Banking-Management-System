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
#include "server.h" 

/* --- CLIENT INTERFACE (I/O & Network Abstraction) --- */
void connect_to_server(const char *ip);
void send_request_and_get_response(int sockfd, request_t *req, response_t *resp);
void customer_menu(int userId, int sockfd, const char* userName);
void employee_menu(int userId, int sockfd, const char* userName);
void manager_menu(int userId, int sockfd, const char* userName);
void admin_menu(int userId, int sockfd, const char* userName);

#endif 