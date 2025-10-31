# Banking Management System

A high-performance, concurrent banking simulation built in C. This project simulates the core functionalities of a bank, featuring a multi-threaded server that handles multiple clients simultaneously, with a strong focus on data integrity and eliminating race conditions.

## 1. Core Technical Features

This isn't just a simple simulation; it's an exercise in robust, low-level system design.

* **⚡ True Concurrency with Record-Level Locking:** The system does *not* lock entire database files during transactions. Instead, it uses `fcntl` to implement **record-level locking** on the `users.db`, `accounts.db`, and `loans.db`. This allows two non-conflicting operations (e.g., Customer A depositing and Customer C withdrawing) to execute in parallel, dramatically increasing throughput.

* **🛡️ Atomic Read-Modify-Write Operations:** All database modifications (e.g., changing a balance, approving a loan, updating a password) are wrapped in atomic helper functions (`atomic_update_account`, `atomic_update_user`, etc.). This pattern acquires a record lock, reads the data, modifies it, and writes it back as a single, indivisible operation, **guaranteeing no race conditions**.

* **🖥️ Multi-Threaded Server:** Built with POSIX threads (`pthread`) and Socket Programming, the server can manage simultaneous connections from multiple clients.

* **🔐 Secure Authentication & Session Management:**
    * **Password Hashing:** Passwords are never stored in plaintext. They are securely hashed using `libcrypt`.
    * [cite_start]**Single Session Enforcement:** The server actively tracks logged-in users and prevents the same account from being logged in more than once at a time. [cite: 29, 41, 52, 60]

## 2. Features by User Role

[cite_start]The system enforces a strict separation of duties through four distinct user roles, each with a specific set of permissions. [cite: 26]

### 👤 Customer
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 1, 38] |
| Deactivated Account Check | [cite_start]✅ | [cite: 2] |
| View Account Balance | [cite_start]✅ | [cite: 30] |
| Deposit Money (Atomic) | [cite_start]✅ | [cite: 31] |
| Withdraw Money (Atomic) | [cite_start]✅ | [cite: 32] |
| Transfer Funds (Atomic) | [cite_start]✅ | [cite: 33] |
| Apply for Loan | [cite_start]✅ | [cite: 34] |
| View Loan Status | [cite_start]✅ | [cite: 5] |
| Add Customer Feedback | [cite_start]✅ | [cite: 36] |
| View Feedback Status | [cite_start]✅ | [cite: 6] |
| View Transaction History | [cite_start]✅ | [cite: 37] |
| Change Password | [cite_start]✅ | [cite: 35] |

### 💼 Bank Employee
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 41, 49] |
| Add New Customer | [cite_start]✅ | [cite: 42] |
| Modify Customer Details | [cite_start]✅ | [cite: 43] |
| Process Loan Applications | [cite_start]✅ | [cite: 44] |
| Approve / Reject Loans | [cite_start]✅ | [cite: 45] |
| *Deactivated Acct. Check* | [cite_start]✅ | [cite: 10, 12] |
| View Assigned Loans | [cite_start]✅ | [cite: 46] |
| View Customer Transactions | [cite_start]✅ | [cite: 47] |
| Change Password | [cite_start]✅ | [cite: 48] |

### 👔 Manager
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 52, 57] |
| Activate / Deactivate Accounts | [cite_start]✅ | [cite: 53] |
| View Non-Assigned Loans | [cite_start]✅ | [cite: 15] |
| Assign Loans to Employees | [cite_start]✅ | [cite: 54] |
| Review Customer Feedback | [cite_start]✅ | [cite: 55] |
| Change Password | [cite_start]✅ | [cite: 56] |

### ⚙️ Administrator
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 60, 65] |
| Add New Bank Employee | [cite_start]✅ | [cite: 61] |
| Modify User Details | [cite_start]✅ | [cite: 62] |
| Manage User Roles | [cite_start]✅ | [cite: 63] |
| Change Password | [cite_start]✅ | [cite: 64] |

## 3. File Structure

The project maintains a clean separation of concerns between header files, source code, and persistent data.
Here is the complete README file, ready to copy and paste.

Markdown

# Banking Management System

A high-performance, concurrent banking simulation built in C. This project simulates the core functionalities of a bank, featuring a multi-threaded server that handles multiple clients simultaneously, with a strong focus on data integrity and eliminating race conditions.

## 1. Core Technical Features

This isn't just a simple simulation; it's an exercise in robust, low-level system design.

* **⚡ True Concurrency with Record-Level Locking:** The system does *not* lock entire database files during transactions. Instead, it uses `fcntl` to implement **record-level locking** on the `users.db`, `accounts.db`, and `loans.db`. This allows two non-conflicting operations (e.g., Customer A depositing and Customer C withdrawing) to execute in parallel, dramatically increasing throughput.

* **🛡️ Atomic Read-Modify-Write Operations:** All database modifications (e.g., changing a balance, approving a loan, updating a password) are wrapped in atomic helper functions (`atomic_update_account`, `atomic_update_user`, etc.). This pattern acquires a record lock, reads the data, modifies it, and writes it back as a single, indivisible operation, **guaranteeing no race conditions**.

* **🖥️ Multi-Threaded Server:** Built with POSIX threads (`pthread`) and Socket Programming, the server can manage simultaneous connections from multiple clients.

* **🔐 Secure Authentication & Session Management:**
    * **Password Hashing:** Passwords are never stored in plaintext. They are securely hashed using `libcrypt`.
    * [cite_start]**Single Session Enforcement:** The server actively tracks logged-in users and prevents the same account from being logged in more than once at a time. [cite: 29, 41, 52, 60]

## 2. Features by User Role

[cite_start]The system enforces a strict separation of duties through four distinct user roles, each with a specific set of permissions. [cite: 26]

### 👤 Customer
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 1, 38] |
| Deactivated Account Check | [cite_start]✅ | [cite: 2] |
| View Account Balance | [cite_start]✅ | [cite: 30] |
| Deposit Money (Atomic) | [cite_start]✅ | [cite: 31] |
| Withdraw Money (Atomic) | [cite_start]✅ | [cite: 32] |
| Transfer Funds (Atomic) | [cite_start]✅ | [cite: 33] |
| Apply for Loan | [cite_start]✅ | [cite: 34] |
| View Loan Status | [cite_start]✅ | [cite: 5] |
| Add Customer Feedback | [cite_start]✅ | [cite: 36] |
| View Feedback Status | [cite_start]✅ | [cite: 6] |
| View Transaction History | [cite_start]✅ | [cite: 37] |
| Change Password | [cite_start]✅ | [cite: 35] |

### 💼 Bank Employee
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 41, 49] |
| Add New Customer | [cite_start]✅ | [cite: 42] |
| Modify Customer Details | [cite_start]✅ | [cite: 43] |
| Process Loan Applications | [cite_start]✅ | [cite: 44] |
| Approve / Reject Loans | [cite_start]✅ | [cite: 45] |
| *Deactivated Acct. Check* | [cite_start]✅ | [cite: 10, 12] |
| View Assigned Loans | [cite_start]✅ | [cite: 46] |
| View Customer Transactions | [cite_start]✅ | [cite: 47] |
| Change Password | [cite_start]✅ | [cite: 48] |

### 👔 Manager
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 52, 57] |
| Activate / Deactivate Accounts | [cite_start]✅ | [cite: 53] |
| View Non-Assigned Loans | [cite_start]✅ | [cite: 15] |
| Assign Loans to Employees | [cite_start]✅ | [cite: 54] |
| Review Customer Feedback | [cite_start]✅ | [cite: 55] |
| Change Password | [cite_start]✅ | [cite: 56] |

### ⚙️ Administrator
| Feature | Status | Source |
| :--- | :--- | :--- |
| Login & Secure Logout | [cite_start]✅ | [cite: 60, 65] |
| Add New Bank Employee | [cite_start]✅ | [cite: 61] |
| Modify User Details | [cite_start]✅ | [cite: 62] |
| Manage User Roles | [cite_start]✅ | [cite: 63] |
| Change Password | [cite_start]✅ | [cite: 64] |

## 3. File Structure

The project maintains a clean separation of concerns between header files, source code, and persistent data.

. ├── include/ # All header files │ ├── admin_module.h │ ├── banking.h │ ├── client.h │ ├── customer_module.h │ ├── employee_module.h │ ├── manager_module.h │ ├── server.h │ └── utils.h ├── src/ # All source code │ ├── admin_module.c │ ├── client.c │ ├── customer_module.c │ ├── employee_module.c │ ├── manager_module.c │ ├── server.c │ ├── utils.c │ ├── bootstrap.c # Utility to seed the database │ └── db_inspector.c # Utility to read the database ├── db/ # Database files are created here (e.g., users.db) └── script.sh # Build script

## 4. How to Compile and Run

This project requires a `gcc` compiler and the `libcrypt` library (for password hashing).

### Step 1: Compile the Project

Use the provided shell script `script.sh` to compile all executables.

```bash
# Make the script executable
chmod +x script.sh

# Run the script
./script.sh