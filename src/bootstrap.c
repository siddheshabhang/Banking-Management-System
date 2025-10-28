/*
 * Save this file as: src/bootstrap.c
 */
#include "admin_module.h" // admin_module.h must be in your include/ dir
#include "utils.h"        // utils.h must be in your include/ dir
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>     // For mkdir

/*
 * This program runs ONCE to create the first admin user.
 * It seeds the users.db file.
 */
int main() {
    printf("Bootstrapping admin user...\n");
    
    // Ensure the db directory exists
    // DB_DIR is defined in server.h, which is included via utils.h
    if (mkdir(DB_DIR, 0700) == -1 && errno != EEXIST) {
        perror("Failed to create db directory");
    } else {
        printf("db directory ensured.\n");
    }
    
    char resp_msg[MAX_MSG_LEN];
    const char *name = "Admin";
    int age = 99;
    const char *address = "127.0.0.1";
    const char *role = "admin";
    const char *username = "admin";
    const char *password = "thealmighty";

    if (add_employee(name, age, address, role, username, password, resp_msg, sizeof(resp_msg))) {
        printf("SUCCESS: %s\n", resp_msg);
        printf("You can now log in with:\n");
        printf("Username: admin\n");
        printf("Password: thealmighty\n");
    } else {
        fprintf(stderr, "FAILURE: %s\n", resp_msg);
    }
    
    return 0;
}
