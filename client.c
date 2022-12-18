#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#include "message.h"

// client code
int main(int argc, char *argv[]) {
    // return 0;
    int x= MFS_Init("localhost",7011);
    assert(x==0);
    x= MFS_Creat(0, 0, "Foo");
    assert(x==0);
    x= MFS_Creat(0, 1, "main.c");
    assert(x==0);
    MFS_Stat_t *m=  malloc(sizeof(MFS_Stat_t));
    x= MFS_Stat(0,m);
    printf("size:: %d\n", m->size);
    printf("type:: %d\n", m->type);
    assert(x==0);
    MFS_Stat_t *n=  malloc(sizeof(MFS_Stat_t));
    x= MFS_Stat(1,n);
    printf("size:: %d\n", n->size);
    printf("type:: %d\n", n->type);
    assert(x==0);
    x = MFS_Shutdown();
    assert(x==0);
    printf("Client Execution successful\n");
}

