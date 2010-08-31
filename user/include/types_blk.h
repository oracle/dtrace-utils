#ifndef TYPES_BLK_H
#define TYPES_BLK_H
#ifdef _LP64
typedef int             blksize_t;      /* used for block sizes */
#else
typedef long            blksize_t;      /* used for block sizes */
#endif
#endif
