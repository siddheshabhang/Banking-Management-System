# 🏦 Banking Management System

## 📜 Description

This project is a comprehensive, multi-threaded, client-server banking management system written entirely in C. It focuses on demonstrating fundamental system software concepts, particularly **concurrency**, **thread synchronization**, **file management using system calls**, and **socket programming**. The system features role-based access control and ensures data consistency through record-level file locking.

## ✨ Features

* **Role-Based Access Control:**
    * **Customer:** Manages their personal account.
    * **Employee:** Manages customer accounts and processes loans.
    * **Manager:** Oversees employees, manages account statuses, and reviews feedback.
    * **Administrator:** Manages all user accounts (customers and employees).
* **Account Management:**
    * Each customer has a **single dedicated account**.
    * View balance.
    * Deposit and withdraw funds.
    * Transfer funds between customers (using Account Numbers).
    * View detailed, timestamped transaction history.
* **Loan System:**
    * Customers can apply for loans.
    * Managers can assign pending loans to employees.
    * Employees can view assigned loans and approve/reject them.
    * Approved loans automatically credit the customer's account.
* **Feedback System:**
    * Customers can submit feedback.
    * Managers can review all unread feedback.
* **User Management:**
    * Employees can add new customers.
    * Employees/Admins can modify user details (KYC info, password).
    * Managers can activate/deactivate customer accounts.
    * Admins can add new employees/managers and change user roles.
* **Concurrency & Security:**
    * **Multithreaded Server:** Handles multiple client connections simultaneously using POSIX threads (`pthread`).
    * **Session Management:** Prevents multiple logins by the same user ID using a mutex-protected global array of active sessions.
    * **File Locking:** Uses `fcntl` for fine-grained **record-level locking** on specific accounts/users (via `atomic_update_` functions) to prevent race conditions and ensure data integrity.
    * **Secure Hashing:** User passwords are securely hashed using `crypt()` (SHA-512).
    * **System Calls:** Prioritizes direct system calls (`open`, `read`, `write`, `lseek`, `fcntl`) over standard library functions for file I/O.

## ⚙️ Technical Requirements Met

* **Socket Programming:** Implements a client-server architecture using TCP sockets.
* **System Calls:** Uses system calls (`open`, `read`, `write`, `lseek`, `fcntl`) for all file management.
* **File Management:** Uses binary files as a database (e.g., `users.db`, `accounts.db`).
* **File Locking:** Implements exclusive (write) locks at the record level for concurrent operations.
* **Multithreading:** Server uses `pthread_create` to spawn a new thread for each client.
* **Synchronization:** Uses `pthread_mutex_t` for session management and `fcntl` locks for file data consistency.

## 🗃️ Data Integrity & ACID Properties

This project implements features to ensure data integrity, modeled after ACID properties.

* **A - Atomicity (All or Nothing):**
    * Atomicity is guaranteed at the level of a *single record modification* (e.g., `atomic_update_user`, `atomic_update_account`).
    * Complex, multi-step transactions (like `transfer_funds`) are made concurrency-safe through sequential record locks. The system ensures that if the deposit step fails, the initial withdrawal is rolled back, maintaining consistency.

* **C - Consistency:**
    * Enforced by **application-level logic** (e.g., checking for sufficient funds in `withdraw_modifier`) and **database constraints** (e.g., `check_uniqueness` for username, email, and phone).

* **I - Isolation:**
    * Implemented using the **`fcntl`** system call for file locking.
    * **Record-Level Locking:** `atomic_update_account` and `atomic_update_user` lock *only* the specific record (byte range) being changed. This allows two users to modify *different* accounts at the same time, providing high throughput.
    * **File-Level Locking:** A whole-file lock is used when searching or appending (e.g., `generate_new_userId`, `check_uniqueness`) to prevent read/write conflicts during table-level operations.

* **D - Durability:**
    * Durability is handled by the operating system's kernel. The `write()` system call guarantees that data is passed to the OS buffer. While `fsync()` is not explicitly called, completed writes will be flushed to disk by the OS, ensuring data persists after the operation is confirmed.

## 🛡️ Robust Error Handling

The system is hardened against common errors and bad user input.

* **Input Validation:** All user input is validated on the client-side before being sent to the server.
    * **Data Type:** `read_valid_double` protects against invalid monetary amounts.
    * **Format:** `validate_phone` (10 digits), `validate_email` (contains `@` and `.`), and `validate_name_part` (no spaces) are used.
    * **Empty Input:** `validate_not_empty` is used for all critical fields.
* **User Experience:**
    * **Re-prompting:** Invalid input (e.g., bad phone number) re-prompts the user for *just that field* instead of aborting the entire operation.
* **Concurrency Safety:**
    * **Race Conditions:** `check_uniqueness` is called within a file lock before creating a new user to prevent two users from being created with the same username, email, or phone.
* **Orphaned Sessions:**
    * The server robustly handles unexpected client disconnects (`Ctrl+C`). The `recv_request()` call will fail, causing the client thread to exit. The thread's cleanup logic calls `remove_active_session()`, freeing the user's slot for a new login.
* **System Call Robustness:**
    * The return values of `read()` and `write()` are checked in `send_request_and_get_response` to detect server disconnects and prevent partial data sends/receives.

## 📁 Project Structure
banking-management-system/
├── include/              # Header files (.h) defining interfaces and structures
│   ├── admin_module.h
│   ├── client.h
│   ├── customer_module.h
│   ├── employee_module.h
│   ├── manager_module.h
│   ├── server.h
│   └── utils.h
│
├── src/                  # Source files (.c) implementing the logic
│   ├── admin_module.c
│   ├── bootstrap.c
│   ├── client.c
│   ├── customer_module.c
│   ├── db_inspector.c
│   ├── employee_module.c
│   ├── manager_module.c
│   ├── server.c
│   └── utils.c
│
├── db/                   # Data files (.db) - (Created by bootstrap)
│   └── (Contains .db files like users.db, accounts.db, etc.)
│
├── script.sh             # Main build script
├── README.md             # This file
├── bootstrap             # (Compiled) Utility to init database
├── inspector             # (Compiled) Utility to read .db files
├── server                # (Compiled) Server executable
└── client                # (Compiled) Client executable

## 🧩 Modules

The project is structured into logical modules:

* **`server.h` / `server.c`:** Core server logic. Handles client connections, threading, login, session management, and dispatches requests to the appropriate role module.
* **`client.h` / `client.c`:** The user-facing program. Provides menus and handles user input validation.
* **`utils.h` / `utils.c`:** Handles all direct file I/O, `fcntl` locking, password hashing, and atomic read-modify-write operations.
* **`customer_module.h` / `.c`:** Implements customer-specific functions (deposit, withdraw, etc.).
* **`employee_module.h` / `.c`:** Implements employee-specific functions (add customer, approve loan, etc.).
* **`manager_module.h` / `.c`:** Implements manager-specific functions (assign loan, review feedback, etc.).
* **`admin_module.h` / `.c`:** Implements admin-specific functions (add employee, modify user, etc.).
* **`bootstrap.c`:** A command-line tool to initialize the database files and create default users.
* **`db_inspector.c`:** A command-line tool to safely read and print the contents of the `.db` files for debugging.

## 🚀 How to Compile and Run

### 1. Compile
A build script is provided for easy compilation on a Linux-based system.

1.  **Open Terminal:** Navigate to the project's root directory.
2.  **Make script executable:**
    ```bash
    chmod +x script.sh
    ```
3.  **Run the build script:**
    ```bash
    ./script.sh
    ```
    This will compile all four required executables: `server`, `client`, `bootstrap`, and `inspector`.

### 2. Run
You will need at least two terminals.

1.  **Initialize Data (Run Once):**
    * Before starting the server for the first time, run the `bootstrap` utility.
    ```bash
    ./bootstrap
    ```
    * This will create the `db/` directory and populate it with default users for all roles. It will print the default credentials to the console.

2.  **Terminal 1: Start the Server**
    ```bash
    ./server
    ```
    *(The server will start and begin listening on port 9090).*

3.  **Terminal 2 (and 3, 4...): Run the Client**
    ```bash
    ./client 127.0.0.1
    ```
    *(Follow the prompts to log in and use the system).*

---
### Default Credentials

The `bootstrap` utility creates the following users:

| Role | Name (ID) | Username | Password |
| :--- | :--- | :--- | :--- |
| Admin | Siddhesh Abhang (1001)| `siddhesh` | `adminpass` |
| Manager | Manasi Joshi (1002) | `manasi` | `managerpass` |
| Employee | Prakash Kadam (1003) | `prakash` | `employeepass` |
| Employee | Smita Pawar (1004) | `smita` | `employeepass` |
| Customer1| Priya Sawant (1005) | `priya` | `customerpass` |
| Customer2| Rohan Deshmukh (1006) | `rohan` | `customerpass` |