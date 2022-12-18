#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#include "message.h"

// client code
int main(int argc, char *argv[]) {
    // return 0;
    int x= MFS_Init("localhost", 10000);
    assert(x==0);
    x= MFS_Creat(0, 1, "testfile");
    assert(x==0);
    x= MFS_Lookup(0, "testdir");
    printf("testdir inum:: %d\n", x);
    // x= MFS_Creat(x, 1 ,"testFile");
    // printf("testFile inum:: %d\n", x);
    char buff[1024] = "START BLOCK";
    x= MFS_Write(x,buff,0,strlen(buff));
    char buff1[]
    // for(int i=0;i<126;i++){
    //     x= MFS_Creat(x,1,itoa(i));
    // }
    // for(int i=0;i<126;i++){
    //     x= MFS_Lookup(0, "testdir");
    // }
    x = MFS_Shutdown();
    assert(x==0);
    printf("Client Execution successful\n");

    // int x= MFS_Init("localhost", 10000);
    // assert(x==0);
    // x= MFS_Lookup(0, "Foo");
    // printf("inodenumber:: %d\n",x);
    // x= MFS_Lookup(0, "main.c");
    // printf("inodenumber:: %d\n",x);
    //  x= MFS_Lookup(0, "./Foo");
    // printf("inodenumber:: %d\n",x);
    // x= MFS_Lookup(0, "../Foo");
    // printf("inodenumber:: %d\n",x);

    // x= MFS_Lookup(0, "/Foo");
    // printf("inodenumber:: %d\n",x);
    //  x= MFS_Lookup(0, "/main.c");
    // printf("inodenumber:: %d\n",x);
}

