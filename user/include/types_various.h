#ifndef TYPES_VARIOUS_H
#define TYPES_VARIOUS_H

#include <sys/types.h>
#include <sys/int_types.h>

typedef struct {                /* syscall set type */
        unsigned int    word[16];
} sysset_t;

typedef id_t    taskid_t;
typedef id_t    projid_t;
typedef id_t    poolid_t;
typedef id_t    zoneid_t;
typedef id_t    ctid_t;

typedef int psetid_t;

typedef uint32_t priv_chunk_t;

typedef struct {	/* return values from system call */
  long	sys_rval1;	/* primary return value from system call */
  long	sys_rval2;	/* second return value from system call */
} sysret_t;

typedef struct rctlblk rctlblk_t;

typedef struct iovec iovec_t;

typedef uint32_t dev32_t;

#define	SHT_SUNW_dof		0x6ffffff4

#define	EM_AMD64	62		/* AMDs x86-64 architecture */

#define	STV_ELIMINATE	6

#define	SHN_SUNW_IGNORE	0xff3f

#define OBJFS_ROOT      "/system/object"

/*
 * Commands to sysinfo(2)
 *
 * Values for sysinfo(2) commands are to be assigned by the following
 * algorithm:
 *
 *    1 -  256  Unix International assigned numbers for `get' style commands.
 *  257 -  512  Unix International assigned numbers for `set' style commands
 *              where the value is selected to be the value for the
 *              corresponding `get' command plus 256.
 *  513 -  768  Solaris specific `get' style commands.
 *  769 - 1024  Solaris specific `set' style commands where the value is
 *              selected to be the value for the corresponding `get' command
 *              plus 256.
 *
 * These values have be registered
 * with Unix International can't be corrected now.  The status of a command
 * as published or unpublished does not alter the algorithm.
 */

/* UI defined `get' commands (1-256) */
#define SI_SYSNAME              1       /* return name of operating system */
#define SI_HOSTNAME             2       /* return name of node */
#define SI_RELEASE              3       /* return release of operating system */
#define SI_VERSION              4       /* return version field of utsname */
#define SI_MACHINE              5       /* return kind of machine */
#define SI_ARCHITECTURE         6       /* return instruction set arch */
#define SI_HW_SERIAL            7       /* return hardware serial number */
#define SI_HW_PROVIDER          8       /* return hardware manufacturer */
#define SI_SRPC_DOMAIN          9       /* return secure RPC domain */


/*
 *      Definitions for commonly used resolutions.
 */
#define SEC             1
#define MILLISEC        1000
#define MICROSEC        1000000
#define NANOSEC         1000000000

/*
 *  * Runtime link-map identifiers.
 *   */
#define LM_ID_BASE              0x00
#define LM_ID_LDSO              0x01
#define LM_ID_NUM               2


#define	SYS_forksys	142

#define	SIGCANCEL 36

#define SIG2STR_MAX     32


#include     <pthread.h>
#define   DEFAULTMUTEX PTHREAD_MUTEX_INITIALIZER


/*
 *  * Definitions of synchronization types.
 *   */
#define USYNC_THREAD    0x00            /* private to a process */
#define USYNC_PROCESS   0x01            /* shared by processes */

#endif
