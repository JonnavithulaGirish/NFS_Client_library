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
	else if(m.mtype == MFS_CRET){
		bitmap_t  *inode_bitmap= image+ (s->inode_bitmap_addr * UFS_BLOCK_SIZE);
		bitmap_t  *data_bitmap= image+ (s->data_region_addr * UFS_BLOCK_SIZE);
		
		//Find free Inode position
		int freeInodeblockPos=0;
		bool foundFreeInode= false;
		for(int i=0;i< s->inode_bitmap_len; i++){
			for(int j=0; j< inode_bitmap->bits;j++){
				if(inode_bitmap->bits[j] & (inode_bitmap->bits[j] +1) != 0){
					int position = getPosition(inode_bitmap->bits[j]);
					unsigned int bitMask = 1 << position;
					inode_bitmap->bits[j] |= bitMask;
					freeInodeblockPos+=(32-position);
					foundFreeInode =true;
					break;
				}
				freeInodeblockPos += 31;
			}
			if(foundFreeInode)
				break;
		}
		freeInodeblockPos-=1;

		//Update Inode with data
		inode_t *new_inode = image + (s->inode_region_addr *UFS_BLOCK_SIZE) + (freeInodeblockPos *UFS_BLOCK_SIZE);
		new_inode->type = m.fileType;

		//Find free Data node position
		int freeDatablockPos=0;
		bool foundFreeDataNode = false;
		for(int i=0;i< s->data_bitmap_len; i++){
			for(int j=0; j< data_bitmap->bits;j++){
				if(data_bitmap->bits[j] & (data_bitmap->bits[j] +1) != 0){
					int position = getPosition(data_bitmap->bits[j]);
					unsigned int bitMask = 1 << position;
					data_bitmap->bits[j] |= bitMask;
					freeDatablockPos+=(32-position);
					break;
				}
				freeDatablockPos += 32;
			}
			if(foundFreeDataNode)
				break;
		}
		freeDatablockPos-=1;
		new_inode->direct[0]= s->data_region_addr + freeDatablockPos;
		new_inode->size = 2* sizeof(dir_ent_t);

		//update data of parentDir
		inode_t* parent_dir_inode = image + (s->inode_region_addr *UFS_BLOCK_SIZE) + (m.inodeNum *UFS_BLOCK_SIZE);
		int directDatablock = parent_dir_inode->size/UFS_BLOCK_SIZE;
		int directDatablockIndex = parent_dir_inode->size % UFS_BLOCK_SIZE;
		if(m.fileType == UFS_DIRECTORY){
			//if type is directory update inode and data for parent
			parent_dir_inode->size += sizeof(dir_ent_t);
			dir_block_t* dir_block = image + (s->data_region_addr*UFS_BLOCK_SIZE) + (directDatablock * UFS_BLOCK_SIZE);
			dir_ent_t newEntry;
			newEntry.inum = freeInodeblockPos;
			strncpy(newEntry.name,  m.path , sizeof(newEntry.name)-1);
			dir_block->dirEntries[directDatablockIndex+1] = newEntry;

			//if type is directory update data new data block
			dir_block_t *dir_node = image + new_inode->direct[0];
			dir_node->dirEntries[0].inum= freeInodeblockPos;
			strncpy(dir_node->dirEntries[0].name,  "." , sizeof(newEntry.name)-1);
			dir_node->dirEntries[1].inum= m.inodeNum;
			strncpy(dir_node->dirEntries[1].name,  ".." , sizeof(newEntry.name)-1);
		}

		

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



int getPosition(unsigned int n){
	int pos = 0;
	unsigned int temp = n;
	int count = 0;

	while(temp > 0){
		if((temp & 1) == 0) pos = count;
		temp = temp >> 1;
		count++;
	}
	return pos;
}



