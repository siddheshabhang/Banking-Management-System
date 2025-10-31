#!/bin/bash

# Compile server.c and other modules
gcc -o server src/server.c src/utils.c src/customer_module.c src/employee_module.c src/manager_module.c src/admin_module.c -Iinclude -pthread

# Compile client.c 
gcc -o client src/client.c -Iinclude

# Compile boostrap.c
gcc -o bootstrap src/bootstrap.c src/admin_module.c src/utils.c src/employee_module.c src/customer_module.c -Iinclude

#Compile inspector.c
gcc -o inspector src/db_inspector.c -Iinclude

echo "######################################################"
echo "  Banking-Management-System compiled successfully!!!   "
echo "######################################################"


#echo "  Starting the Bank Server  "
#read -p "  Enter port number for Server:  " PORT
#echo

#echo "  Starting the server on port $PORT..."
#./server PORT

