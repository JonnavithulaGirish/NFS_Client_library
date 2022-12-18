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
int getPosition(unsigned int n);
int  lookupHelperFun(inode_t inode,super_t *s,message_t m);
bool checkValidWrite(message_t m,super_t *s);

// server code
int main(int argc, char *argv[]) {
    int sd = UDP_Open(7011);
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
	inode_t *inode_table = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
	if(m.mtype== MFS_INIT){
		res_m.rootInodeNum = 0;
	}
	else if(m.mtype == MFS_CRET){
		bitmap_t  *inode_bitmap= image+ (s->inode_bitmap_addr * UFS_BLOCK_SIZE);
		bitmap_t  *data_bitmap= image+ (s->data_bitmap_addr * UFS_BLOCK_SIZE);
		//Find free Inode position
		int freeInodeblockPos=0;
		bool foundFreeInode= false;
		for(int i=0;i< s->inode_bitmap_len; i++){
			for(int j=0; j <1024;j++){
				unsigned int temp = inode_bitmap->bits[0]&(inode_bitmap->bits[0] +1);
				if(temp != 0){
					int position = getPosition(inode_bitmap->bits[j]);
					unsigned int bitMask = 1 << position;
					inode_bitmap->bits[j] |= bitMask;
					freeInodeblockPos+=(32-position);
					foundFreeInode =true;
					break;
				}
				freeInodeblockPos += 32;
			}
			if(foundFreeInode)
				break;
		}
		freeInodeblockPos-=1;
		printf("freeInodeblockPos: %d\n", freeInodeblockPos);

		//Update Inode with data
		inode_t new_inode = inode_table[freeInodeblockPos];
		new_inode.type = m.fileType;

		// //Find free Data node position
		int freeDatablockPos=0;
		bool foundFreeDataNode = false;
		for(int i=0;i< s->data_bitmap_len; i++){
			for(int j=0; j< 1024;j++){
				unsigned int temp = data_bitmap->bits[j]&(data_bitmap->bits[j]+1);
				if(temp != 0){
					int position = getPosition(data_bitmap->bits[j]);
					unsigned int bitMask = 1 << position;
					data_bitmap->bits[j] |= bitMask;
					freeDatablockPos+=(32-position);
					break;
				}
			}
			if(foundFreeDataNode)
				break;
		}
		freeDatablockPos-=1;
		printf("freeDatablockPos: %d\n", freeDatablockPos);
		new_inode.direct[0]= s->data_region_addr + freeDatablockPos;


		//update data of parentDir
		inode_t parent_dir_inode = inode_table[m.inodeNum];
		int directDatablock = parent_dir_inode.size/UFS_BLOCK_SIZE;
		int directDatablockIndex = (parent_dir_inode.size % UFS_BLOCK_SIZE )/sizeof(dir_ent_t);
		
		//if type is directory/file update inode and data for parent
		parent_dir_inode.size += sizeof(dir_ent_t);
		dir_block_t* parent_dir_data_block = image + (parent_dir_inode.direct[directDatablock] * UFS_BLOCK_SIZE);
		parent_dir_data_block->dirEntries[directDatablockIndex].inum= freeInodeblockPos;
		strcpy(parent_dir_data_block->dirEntries[directDatablockIndex].name,  m.path);

		if(m.fileType == UFS_DIRECTORY){
			//if type is directory update data new data block
			new_inode.size = 2* sizeof(dir_ent_t);
			dir_block_t * new_dir_data_block = image +  + (s->data_region_addr*UFS_BLOCK_SIZE) + (new_inode.direct[0] * UFS_BLOCK_SIZE);
			new_dir_data_block->dirEntries[0].inum= freeInodeblockPos;
			strcpy(new_dir_data_block->dirEntries[0].name,  "." );
			new_dir_data_block->dirEntries[1].inum= m.inodeNum;
			strcpy(new_dir_data_block->dirEntries[1].name,  "..");
			printf("root inode type:: %d, size:: %d ", inode_table[m.inodeNum].type , inode_table[m.inodeNum].size);
			printf("inode type:: %d, size:: %d ", new_inode.type, new_inode.size);
		}
		rc = fsync(fd);
    	assert(rc > -1);
		
	}else if(m.mtype== MFS_LOOKUP){
		if(m.inodeNum<0 || m.inodeNum>= (s->inode_region_len*(UFS_BLOCK_SIZE))/sizeof(inode_t)){
			res_m.mtype= MFS_LOOKUP;
			res_m.rc = -1;
		}else{
			inode_t inode = inode_table[m.inodeNum];
			if(inode.type == 1){
				res_m.mtype= MFS_LOOKUP;
				res_m.rc = -1;
			}else{
				char * token = strtok(m.path,"/");
				while(token != NULL){
					message_t tempMsg = m;
					strcpy(tempMsg.path,token);
					int new_inodeNum = lookupHelperFun(inode,s,tempMsg);
					if(new_inodeNum == -1){
						res_m.inodeNum = -1;
						res_m.rc = -1;
						break;
					}
					res_m.inodeNum = new_inodeNum;
					res_m.rc = 1;
					inode = inode_table[new_inodeNum];
					token = strtok(NULL, " ");
				}
				
			}
			
		}	
	}else if(m.mtype == MFS_WRITE){
		bitmap_t  *data_bitmap= image+ (s->data_bitmap_addr * UFS_BLOCK_SIZE);
		if(!checkValidWrite(m,s)){
			res_m.mtype= MFS_WRITE;
			res_m.rc = -1;
		}else{
			inode_t* inode = &inode_table[m.inodeNum];
			if(inode->type == 1){
				res_m.mtype= MFS_WRITE;
				res_m.rc = -1;
			}else if(m.offset<0 || m.offset>inode->size){
				res_m.mtype= MFS_WRITE;
				res_m.rc = -1;
			}else{
				int r = m.offset/UFS_BLOCK_SIZE;
				int c = m.offset%UFS_BLOCK_SIZE;	
				int extra = m.offset+m.nbytes - inode->size;
				extra = extra>0?extra:0;
				char* datablock = (char*) image + inode->direct[r]*UFS_BLOCK_SIZE;
				int i = m.offset;
				int n_bytes = m.nbytes;
				//update the size of the inode;
				inode->size = inode->size+extra;
				int count = 0;
				while(i<4096 && n_bytes>0){
					//need to check how to update the bytes;
					datablock[i] = m.buffer[i-m.offset];
					count++;
					i++;
					n_bytes--;
				}
				if(n_bytes != 0){
					//need to get a new data block from the data bit map and update the data bit map.
					int freeDatablockPos=0;
					for(int j=0; j< 1024* s->data_bitmap_len; j++){
						unsigned int temp = data_bitmap->bits[j]&(data_bitmap->bits[j]+1);
						if(temp != 0){
							int position = getPosition(data_bitmap->bits[j]);
							unsigned int bitMask = 1 << position;
							data_bitmap->bits[j] |= bitMask;
							freeDatablockPos+=(32-position);
							break;
						}
						freeDatablockPos += 32;
					}
					freeDatablockPos-=1;
					//need to update a data block for the inode.(r+1)
					inode->direct[r+1]= s->data_region_addr+(freeDatablockPos);

					int i=0;
					datablock = (char*) image + inode->direct[r+1]*UFS_BLOCK_SIZE;
					//need to update the remaining bytes to the new data block.
					while(i<4096 && n_bytes>0){
						//need to check how to update the bytes;
						datablock[i] = m.buffer[i+count];
						i++;
						n_bytes--;
					}
				}
				res_m.rc = 1;
				
			}

		}
	}else if(m.mtype == MFS_READ){
		bitmap_t  *data_bitmap= image+ (s->data_bitmap_addr * UFS_BLOCK_SIZE);
		if(!checkValidWrite(m,s)){
			res_m.mtype= MFS_READ;
			res_m.rc = -1;
		}else{
			inode_t* inode = &inode_table[m.inodeNum];
			if(m.offset<0 || m.offset+m.nbytes<inode->size){
				res_m.mtype= MFS_READ;
				res_m.rc = -1;
			}else{
				int r = m.offset/UFS_BLOCK_SIZE;
				int c = m.offset%UFS_BLOCK_SIZE;	
				// char* datablock = (char*) image+ s->data_region_addr+ inode->direct[r]*UFS_BLOCK_SIZE;
				char* datablock = (char*) image + inode->direct[r]*UFS_BLOCK_SIZE;
				int i = m.offset;
				int n_bytes = m.nbytes;
				int count = 0;
				while(i<4096 && n_bytes>0){
					//need to check how to update the bytes;
					// datablock[i] = m.buffer[i-m.offset];
					m.buffer[i-m.offset] = datablock[i];
					i++;
					count++;
					n_bytes--;
				}
				if(n_bytes != 0){
					int i=0;
					datablock = (char*) image + inode->direct[r+1]*UFS_BLOCK_SIZE;

					//need to update the remaining bytes to the new data block.
					while(i<4096 && n_bytes>0){
						//need to check how to update the bytes;
						// datablock[i] = m.buffer[i];
						res_m.buffer[i+count] = datablock[i];
						i++;
						n_bytes--;
					}
				}
				res_m.rc = 1;
				
			}

		}
	}else if(m.mtype == MFS_SHUTDOWN){
		res_m.mtype= MFS_SHUTDOWN;
		res_m.rc =1;
		rc = fsync(fd);
    	assert(rc > -1);
	}
	else if(m.mtype == MFS_STAT){
		inode_t *inode_table = image + (s->inode_region_addr * UFS_BLOCK_SIZE) + (m.inodeNum*sizeof(inode_t));
		res_m.fStats.type= inode_table->type;
		res_m.fStats.size = inode_table->size;
		res_m.rc =1;
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




int  lookupHelperFun(inode_t inode,super_t *s,message_t m){
	int datablocks = 0;
	if(inode.size %UFS_BLOCK_SIZE ==0){
		datablocks = inode.size/UFS_BLOCK_SIZE;
	}else{
		datablocks = 1 + inode.size/UFS_BLOCK_SIZE;
	}
	for(int i=0;i<datablocks;i++){
		dir_ent_t* dataBlock  = (dir_ent_t*) inode.direct[i];
		if(i!=datablocks-1){
			int maxDirEntries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
			for(int j=0;j<maxDirEntries;j++){
				dir_ent_t resInode = dataBlock[j];
				if(strcmp(resInode.name,m.path) == 0){
					return resInode.inum;
				}
			}
		}else{
			int x = inode.size %UFS_BLOCK_SIZE;
			int maxDirEntries = x/sizeof(dir_ent_t);
			for(int j=0;j<maxDirEntries;j++){
				dir_ent_t resInode = dataBlock[j];
				if(strcmp(resInode.name,m.path) == 0){
					return resInode.inum;
				}
			}

		}
	}

	return -1;
}

bool checkValidWrite(message_t m,super_t *s){
	if(m.inodeNum<0 
		|| m.inodeNum>= (s->inode_region_len*(UFS_BLOCK_SIZE))/sizeof(inode_t)
		|| m.nbytes >4096
		){
			return false;

		}
	return true;
}


