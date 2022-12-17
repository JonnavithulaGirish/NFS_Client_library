#include "udp.c"
#include "ufs.h"
#include "message.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>


message_t FileSystemUpdate(message_t m);

// server code
int main(int argc, char *argv[]) {
    int sd = UDP_Open(10000);
    assert(sd > -1);
    while (1) {
		struct sockaddr_in addr;
		message_t m;
		printf("server:: waiting...\n");
		int rc = UDP_Read(sd, &addr, (char *) &m, sizeof(message_t));
		printf("server:: read message [size:%d contents:(%d)]\n", rc, m.mtype);
		if (rc > 0) {
			message_t res_m = FileSystemUpdate(m);
			rc = UDP_Write(sd, &addr, (char *) &res_m, sizeof(message_t));
			printf("server:: reply\n");
			if(res_m.mtype == MFS_SHUTDOWN){
				printf("Shutting down server");
				exit(0);
			}
		} 
    }
    return 0; 
}

 
message_t FileSystemUpdate(message_t m){
	message_t res_m;
	int fd = open("test.img", O_RDWR);
    assert(fd > -1);
    struct stat sbuf;
    int rc = fstat(fd, &sbuf);
    assert(rc > -1);
    int image_size = (int) sbuf.st_size;
    void *image = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(image != MAP_FAILED);

    super_t *s = (super_t *) image;
	if(m.mtype== MFS_INIT){
		inode_t *inode_table = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
		res_m.rootInodeNum = 0;
	}
	else if(m.mtype == MFS_SHUTDOWN){
		res_m.mtype= MFS_SHUTDOWN;
		res_m.rc =1;
		rc = msync(s, sizeof(super_t), MS_SYNC);
    	assert(rc > -1);
	}
    close(fd);
    return res_m;
}
