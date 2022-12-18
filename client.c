#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#include "message.h"

// client code
int main(int argc, char *argv[]) {
    // return 0;
    int x= MFS_Init("localhost", 10000);
    assert(x==0);
    x= MFS_Creat(0, 0, "testfolder");
    assert(x==0);
    int inum = MFS_Lookup(0,"testfolder");
    printf("testfolder inodenum:: %d\n",inum);
    for(int i=0;i<126;i++){
        char int_str[30];
        sprintf(int_str, "%d", i);
        x= MFS_Creat(inum, 1, int_str);
        assert(x==0);
    }
    for(int i=0;i<126;i++){
        char int_str[30];
        sprintf(int_str, "%d", i);
        x= MFS_Lookup(inum,int_str);
        printf("lol inodenum:: %d\n",x);
    }
    x = MFS_Shutdown();
    assert(x==0);
}

