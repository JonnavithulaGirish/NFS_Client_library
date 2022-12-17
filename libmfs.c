#include <stdio.h>
#include "mfs.h"
#include "udp.c"
#include "message.h"
#include <limits.h>
#include <stdbool.h>


struct sockaddr_in addrSnd, addrRcv;
int sd,rc;
int rootInodeNum;
bool connectionEstablished;


int createUdpConnection(char* hostname, int port){
    sd = UDP_Open(20000);
    rc = UDP_FillSockAddr(&addrSnd, hostname, port);
    assert(sd>0 && rc>=0);
    connectionEstablished= true;
    return 1;
}

message_t sendMessageToServer(message_t m){
    printf("client:: send message %d\n", m.mtype);
    rc = UDP_Write(sd, &addrSnd, (char *) &m, sizeof(message_t));
    if (rc < 0) {
	    printf("client:: failed to send\n");
	    exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Client_Read(sd, &addrRcv, (char *) &m, sizeof(message_t));
    if(rc<0){
        m.retry =true;
        printf("Client:: did not receive data from server\n");
        return m;
    }
    printf("client:: got reply [size:%d rc:%d type:%d]\n", rc, m.rc, m.mtype);
    return m;
}


int MFS_Init(char *hostname, int port) {
    printf("MFS Init2 %s %d\n", hostname, port);
    message_t m,res_m;
    m.mtype= MFS_INIT;
    if(createUdpConnection(hostname,port)>0){
        while(true){
            res_m= sendMessageToServer(m);
            if(!res_m.retry)
                break;
            else{
                printf("Retrying MFS_Init\n");
            }
        }
        rootInodeNum = res_m.rootInodeNum;
        printf("Conncetion was established with Server & rootInodeNum is : %d\n", rootInodeNum);
    }
    else{
        return -1;
    }
    return 0;
}

int MFS_Lookup(int pinum, char *name) {
    // network communication to do the lookup to server
    return 0;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
    return 0;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes) {
    return 0;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes) {
    return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
    return 0;
}

int MFS_Unlink(int pinum, char *name) {
    return 0;
}

int MFS_Shutdown() {
    if(connectionEstablished){
        message_t m,res_m;
        m.mtype= MFS_SHUTDOWN;
        while(true){
            res_m= sendMessageToServer(m);
            if(!res_m.retry)
                break;
            else{
                printf("Retrying MFS_Shutdown\n");
            }
        }
        assert(res_m.retry== false && res_m.rc==1);
        printf("Server shutdown successful\n");
    }    
    else{
        return -1;
    }
    return 0;
}

