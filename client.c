#include <stdio.h>
#include "udp.h"
#include "mfs.h"

#include "message.h"

// client code
int main(int argc, char *argv[]) {
    // return 0;
    int x= MFS_Init("localhost", 10000);
    assert(x==0);
    x = MFS_Shutdown();
    assert(x==0);
    printf("Client Execution successful\n");
}

