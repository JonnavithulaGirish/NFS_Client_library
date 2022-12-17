#include "mfs.h"
#include <stdbool.h>
#ifndef __message_h__
#define __message_h__

#define MFS_INIT (1)
#define MFS_LOOKUP (2)
#define MFS_STAT (3)
#define MFS_WRITE (4)
#define MFS_READ (5)
#define MFS_CRET (6)
#define MFS_UNLINK (7)
#define MFS_SHUTDOWN (8)


typedef struct {
    int mtype; // message type from above
    int rc;    // return code
    int rootInodeNum;
    int inodeNum;
    char* path;
    MFS_Stat_t fStats;
    char *buffer;
    int offset;
    int nbytes;
    int fileType;
    bool retry;
} message_t;

#endif // __message_h__
