#ifndef _32_SYS_STAT_H
#define _32_SYS_STAT_H

#include "/usr/include/sys/stat.h"
#include "/usr/include/sys/types.h"
#include <types_posix.h>
#include <types_various.h>

#ifndef __USE_LARGEFILE64
	
#define stat64 stat
 	
#endif

#if defined(_SYSCALL32)

/*
 * Kernel's view of user ILP32 stat and stat64 structures
 */

struct stat32 {
        dev32_t         st_dev;
        int32_t         st_pad1[3];
        ino32_t         st_ino;
        mode32_t        st_mode;
        nlink32_t       st_nlink;
        uid32_t         st_uid;
        gid32_t         st_gid;
        dev32_t         st_rdev;
        int32_t         st_pad2[2];
        off32_t         st_size;
        int32_t         st_pad3;
        timestruc32_t   st_atim;
        timestruc32_t   st_mtim;
        timestruc32_t   st_ctim;
        int32_t         st_blksize;
        blkcnt32_t      st_blocks;
        char            st_fstype[_ST_FSTYPSZ];
        int32_t         st_pad4[8];
};

#if _LONG_LONG_ALIGNMENT == 8 && _LONG_LONG_ALIGNMENT_32 == 4
#pragma pack(4)
#endif

struct stat64_32 {
        dev32_t         st_dev;
        int32_t         st_pad1[3];
        ino64_t         st_ino;
        mode32_t        st_mode;
        nlink32_t       st_nlink;
        uid32_t         st_uid;
        gid32_t         st_gid;
        dev32_t         st_rdev;
        int32_t         st_pad2[2];
        off64_t         st_size;
        timestruc32_t   st_atim;
        timestruc32_t   st_mtim;
        timestruc32_t   st_ctim;
        int32_t         st_blksize;
        blkcnt64_t      st_blocks;
        char            st_fstype[_ST_FSTYPSZ];
        int32_t         st_pad4[8];
};

#if _LONG_LONG_ALIGNMENT == 8 && _LONG_LONG_ALIGNMENT_32 == 4
#pragma pack()
#endif

#endif /*_SYSCALL32*/

#endif
