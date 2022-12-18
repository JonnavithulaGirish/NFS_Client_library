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
#include <stdbool.h>
#include <signal.h>

message_t FileSystemUpdate(message_t m, char* path);
int getPosition(unsigned int n);
int lookupHelperFun(inode_t* inode, super_t *s,message_t m, void* image);
bool isInodeNumberValid(message_t m, super_t *s,bitmap_t *inode_bitmap);
bool checkValidWrite(message_t m,super_t *s);
void intHandler(int dummy);
int sd;
void unlinkHelperFunc(int inodeIndex,bitmap_t  *inode_bitmap,bitmap_t  *data_bitmap,inode_t* inode);

// server code
int main(int argc, char *argv[]) {
	if(argc<2){
		exit(0);
	}
	else{
		int port  = atoi(argv[1]);
		char *path = malloc(sizeof(char)*500);
		strcpy(path, argv[2]);
		sd = UDP_Open(port);
		signal(SIGINT, intHandler);
		assert(sd > -1);
		while (1) {
			struct sockaddr_in addr;
			message_t m;
			printf("server:: waiting...\n");
			int rc = UDP_Read(sd, &addr, (char *) &m, sizeof(message_t));
			printf("server:: read message [size:%d contents:(%d)]\n", rc, m.mtype);
			if (rc > 0) {
				message_t res_m = FileSystemUpdate(m, path);
				rc = UDP_Write(sd, &addr, (char *) &res_m, sizeof(message_t));
				printf("server:: reply\n");
				if(res_m.mtype == MFS_SHUTDOWN){
					while(true){
						if(UDP_Close(sd) ==0)
							break;
					}
					printf("Shutting down server");
					exit(0);
				}
			} 
		}
		return 0; 
	}
}

 
message_t FileSystemUpdate(message_t m, char* path){
	message_t res_m;
	int fd = open(path, O_RDWR);
    assert(fd > -1);
    struct stat sbuf;
    int rc = fstat(fd, &sbuf);
    assert(rc > -1);
    int image_size = (int) sbuf.st_size;
    void *image = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    assert(image != MAP_FAILED);
    super_t *s = (super_t *) image;
	inode_t *inode_table = image + (s->inode_region_addr * UFS_BLOCK_SIZE);
	bitmap_t  *inode_bitmap= image+ (s->inode_bitmap_addr * UFS_BLOCK_SIZE);
	bitmap_t  *data_bitmap= image+ (s->data_bitmap_addr * UFS_BLOCK_SIZE);
	if(m.mtype== MFS_INIT){
		res_m.rootInodeNum = 0;
	}
	else if(m.mtype == MFS_CRET){
		if(!isInodeNumberValid(m, s, inode_bitmap) || inode_table[m.inodeNum].type == 1){
			res_m.mtype= MFS_CRET;
			res_m.rc = -1;
			printf("madda kuduv");
		}
		else{
			//Find free Inode position
			int freeInodeblockPos=0;
			for(int j=0; j < (1024 * s->inode_bitmap_len); j++){
				if(inode_bitmap->bits[j] == 0){
					inode_bitmap->bits[j] = 2147483648;
					freeInodeblockPos+=1;
					break;
				}
				unsigned int temp = inode_bitmap->bits[j]&(inode_bitmap->bits[j] +1);
				//printf("inode temp:: %u",temp);
				if(temp != 0){
					int position = getPosition(inode_bitmap->bits[j]);
					unsigned int bitMask = 1 << position;
					inode_bitmap->bits[j] |= bitMask;
					freeInodeblockPos+=(32-position);
					break;
				}
				freeInodeblockPos += 32;
			}
			freeInodeblockPos-=1;
			res_m.inodeNum = freeInodeblockPos;
			printf("freeInodeblockPos: %d\n", freeInodeblockPos);

			//Update Inode with data
			inode_t *new_inode = &inode_table[freeInodeblockPos];
			new_inode->type = m.fileType;

			// //Find free Data node position
			int freeDatablockPos=0;
			for(int j=0; j< 1024* s->data_bitmap_len; j++){
				if(data_bitmap->bits[j] == 0){
					data_bitmap->bits[j] = 2147483648;
					freeDatablockPos+=1;
					break;
				}
				unsigned int temp = data_bitmap->bits[j]&(data_bitmap->bits[j]+1);
				//printf("data temp:: %u",temp);
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
			printf("freeDatablockPos: %d\n", freeDatablockPos);
			new_inode->direct[0]= s->data_region_addr + freeDatablockPos;


			//update data of parentDir
			inode_t* parent_dir_inode = &inode_table[m.inodeNum];
			int directDatablock = parent_dir_inode->size/UFS_BLOCK_SIZE;
			int directDatablockIndex = (parent_dir_inode->size % UFS_BLOCK_SIZE )/sizeof(dir_ent_t);
			
			//if type is directory/file update inode and data for parent
			parent_dir_inode->size += sizeof(dir_ent_t);
			//printf("update parent direct size: %d/n",parent_dir_inode->size);
			dir_block_t* parent_dir_data_block = image + (parent_dir_inode->direct[directDatablock] * UFS_BLOCK_SIZE);
			parent_dir_data_block->dirEntries[directDatablockIndex].inum= freeInodeblockPos;
			strcpy(parent_dir_data_block->dirEntries[directDatablockIndex].name,  m.path);
			//printf("file type:  %d\n", m.fileType);
			if(m.fileType == UFS_DIRECTORY){
				//if type is directory update data new data block
				new_inode->size = 2* sizeof(dir_ent_t);
				dir_block_t * new_dir_data_block = image + (new_inode->direct[0] * UFS_BLOCK_SIZE);
				new_dir_data_block->dirEntries[0].inum= freeInodeblockPos;
				strcpy(new_dir_data_block->dirEntries[0].name,  "." );
				new_dir_data_block->dirEntries[1].inum= m.inodeNum;
				strcpy(new_dir_data_block->dirEntries[1].name,  "..");
				printf("inode type:: %d, size:: %d\n", new_inode->type, new_inode->size);
			}
			printf("root inode type:: %d, size:: %d\n", inode_table[m.inodeNum].type , inode_table[m.inodeNum].size);
			rc = fsync(fd);
			assert(rc > -1);
		}
	}
	else if(m.mtype == MFS_SHUTDOWN){
		res_m.mtype= MFS_SHUTDOWN;
		res_m.rc =1;
		rc = fsync(fd);
    	assert(rc > -1);
	}else if(m.mtype == MFS_WRITE){
		bitmap_t  *data_bitmap= image+ (s->data_bitmap_addr * UFS_BLOCK_SIZE);
		if(!checkValidWrite(m,s)){
			printf("Hi in the write check condition\n");
			res_m.mtype= MFS_WRITE;
			res_m.rc = -4;
		}else{
			inode_t* inode = &inode_table[m.inodeNum];
			//if it is 0 then it is a directory or else it is a file.
			if(inode->type == 0){
				printf("Hi in the type check\n");
				res_m.mtype= MFS_WRITE;
				res_m.rc = -2;
			}else if(m.offset<0 || m.offset>inode->size){
				printf("Hi in offset check\n");
				res_m.mtype= MFS_WRITE;
				res_m.rc = -3;
			}else{
				int r = m.offset/UFS_BLOCK_SIZE;
				if(r>=30){
					printf("Hi in maxfilesize check\n");
					res_m.mtype= MFS_WRITE;
					res_m.rc = -4;
				}
				else{
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
						datablock[i] = m.buffer[count];
						res_m.buffer[count] = datablock[i];
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
							datablock[i] = m.buffer[count];
							i++;
							count++;
							n_bytes--;
						}
					}
					res_m.mtype= MFS_WRITE;
					res_m.rc = 1;
					rc = fsync(fd);
					assert(rc > -1);
				}
			}

		}
	}else if(m.mtype == MFS_READ){
		bitmap_t  *data_bitmap= image+ (s->data_bitmap_addr * UFS_BLOCK_SIZE);
		if(!checkValidWrite(m,s)){
			res_m.mtype= MFS_READ;
			res_m.rc = -2;
		}else{
			inode_t* inode = &inode_table[m.inodeNum];
			if(m.offset<0 || m.offset+m.nbytes>inode->size){
				res_m.mtype= MFS_READ;
				res_m.rc = -3;
			}else{
				int r = m.offset/UFS_BLOCK_SIZE;
				int c = m.offset%UFS_BLOCK_SIZE;	
				if(r>=30){
					printf("Hi in maxfilesize check\n");
					res_m.mtype= MFS_WRITE;
					res_m.rc = -4;
				}
				else{
				// char* datablock = (char*) image+ s->data_region_addr+ inode->direct[r]*UFS_BLOCK_SIZE;
					char* datablock = (char*) image + inode->direct[r]*UFS_BLOCK_SIZE;
					int i = m.offset;
					int n_bytes = m.nbytes;
					int count = 0;
					while(i<4096 && n_bytes>0){
						//need to check how to update the bytes;
						// datablock[i] = m.buffer[i-m.offset];
						res_m.buffer[count] = datablock[i];
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
							res_m.buffer[count] = datablock[i];
							i++;
							count++;
							n_bytes--;
						}
					}
					res_m.mtype = MFS_READ;
					res_m.rc = 1;
				}
				
			}

		}
	}
	else if(m.mtype== MFS_LOOKUP){

		if(!isInodeNumberValid(m, s, inode_bitmap)){
			//printf("Hi in the inode checker\n");
			res_m.mtype= MFS_LOOKUP;
			res_m.rc = -1;
		}else{
			inode_t* inode = &inode_table[m.inodeNum];
			if(inode->type == 1){
				//printf("Hi in the file lookup\n");
				res_m.mtype= MFS_LOOKUP;
				res_m.rc = -1;
			}else{
				char * token = strtok(m.path,"/");
				while(token != NULL){
					message_t tempMsg = m;
					strcpy(tempMsg.path,token);
					//printf("Hi in lookup server ---%d %d %s:\n",inode->size,inode->type,tempMsg.path);
					int new_inodeNum = lookupHelperFun(inode,s,tempMsg,image);
					if(new_inodeNum == -1){
						res_m.inodeNum = -1;
						res_m.rc = -1;
						break;
					}
					res_m.inodeNum = new_inodeNum;
					res_m.rc = 1;
					inode = &inode_table[new_inodeNum];
					token = strtok(NULL, " ");
				}
				
			}
			
		}	
	}
	else if(m.mtype == MFS_STAT){
		inode_t *inode_table = image + (s->inode_region_addr * UFS_BLOCK_SIZE) + (m.inodeNum*sizeof(inode_t));
		res_m.fStats.type= inode_table->type;
		res_m.fStats.size = inode_table->size;
		res_m.rc =1;
	}else if(m.mtype == MFS_UNLINK){
		if(!isInodeNumberValid(m, s, inode_bitmap)){
			// printf("Hi in the inode checker\n");
			res_m.mtype= MFS_UNLINK;
			res_m.rc = -1;
		}else{
			inode_t* inode = &inode_table[m.inodeNum];
			inode_t* childInode;
			if(inode->type == 1){
				res_m.mtype= MFS_UNLINK;
				res_m.rc = -1;
			}else{
				message_t tempMsg = m;
				strcpy(tempMsg.path,m.path);
				int childInodeNum = lookupHelperFun(inode,s,tempMsg,image);
				if(childInodeNum == -1){
					res_m.mtype= MFS_UNLINK;
					res_m.rc = 1;
				}else{
					childInode = &inode_table[childInodeNum];
					if(childInode->type == 0 && childInode->size != 64){
						res_m.mtype= MFS_UNLINK;
						res_m.rc = -1;
					}else{
						inode->size -=32;
						unlinkHelperFunc(childInodeNum,inode_bitmap,data_bitmap,childInode);
						rc = fsync(fd);
						assert(rc > -1);
						res_m.mtype= MFS_UNLINK;
						res_m.rc = 1;	
					}
				}
			}
			
		}
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

bool isInodeNumberValid(message_t m, super_t *s,bitmap_t *inode_bitmap){
	if(m.inodeNum<0 || m.inodeNum>= (s->inode_region_len*(UFS_BLOCK_SIZE))/sizeof(inode_t)){
			return false;
	}
	// is present in bitmap or not
	// int test=  m.inodeNum%32 == 0 ? (m.inodeNum/32)+1 : m.inodeNum/32;
	// int pos = m.inodeNum%32;
	// if(inode_bitmap->bits[test]>>(32-pos)&(1) % 2 == 1){	
	// 	return false;
	// }
	return true;
}

 
int lookupHelperFun(inode_t* inode, super_t *s,message_t m, void* image){
	int datablocks = 0;
	// printf("node : %d\n",inode.);

	if(inode->size %UFS_BLOCK_SIZE ==0){
		datablocks = inode->size/UFS_BLOCK_SIZE;
	}else{
		datablocks = 1 + inode->size/UFS_BLOCK_SIZE;
	}

	for(int i=0;i<datablocks;i++){
		dir_ent_t* dataBlock  = (dir_ent_t*)( image + (inode->direct[i] * UFS_BLOCK_SIZE));
		if(i!=datablocks-1){
			int maxDirEntries = UFS_BLOCK_SIZE/sizeof(dir_ent_t);
			for(int j=0;j<maxDirEntries;j++){
				dir_ent_t resInode = dataBlock[j];
				if(strcmp(resInode.name,m.path) == 0){
					return resInode.inum;
				}
			}
		}else{
			int x = inode->size %UFS_BLOCK_SIZE ==0 ? UFS_BLOCK_SIZE : inode->size %UFS_BLOCK_SIZE;
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

void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}

void unlinkHelperFunc(int inodeIndex,bitmap_t  *inode_bitmap,bitmap_t  *data_bitmap,inode_t* inode){
	int r = inodeIndex/(4096*8);
	int c = inodeIndex%(4096*8);
	inode_bitmap =inode_bitmap+r;
	int r1 = c/8;
	int c1 = c%8;
	char* temp = (char*)inode_bitmap;
	temp = temp + r1;
	char val  = *temp;
	int  pos = 1<<c1;
	*temp = *temp^pos;	

	int datablocks = 0;
	// printf("node : %d\n",inode.);
	if(inode->size %UFS_BLOCK_SIZE ==0){
		datablocks = inode->size/UFS_BLOCK_SIZE;
	}else{
		datablocks = 1 + inode->size/UFS_BLOCK_SIZE;
	}
	
	for(int i=0;i<datablocks;i++){
		int dataBlockIndex= inode->direct[i];

		int r = dataBlockIndex/(4096*8);
		int c = dataBlockIndex%(4096*8);
		data_bitmap =data_bitmap+r;
		int r1 = c/8;
		int c1 = c%8;
		char* temp = (char*)data_bitmap;
		temp = temp + r1;
		char val  = *temp;
		int  pos = 1<<c1;
		*temp = *temp^pos;	
	}


}


bool checkValidWrite(message_t m,super_t *s){
	if(m.inodeNum<0 
		|| m.inodeNum>= (s->inode_region_len*(UFS_BLOCK_SIZE))/sizeof(inode_t)
		|| m.nbytes >4096
		){
			printf("Hi in the checkCondition\n");
			return false;

		}
	return true;
}