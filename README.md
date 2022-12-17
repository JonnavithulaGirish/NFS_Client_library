# NFS_Client_library

2 Applications - Client and Server

Client uses NFS_Client_library libmfs.so to communicate with server application

Compile libmfs 
1) make -f Makefile.libmfs

Compile and run client
1) gcc -o client client.c -Wall -L. -lmfs
2) export LD_LIBRARY_PATH=.
3) ./client

Complie and run server
1) gcc -o server server.c
2) ./server
