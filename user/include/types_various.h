#ifndef TYPES_VARIOUS_H
#define TYPES_VARIOUS_H

#include <sys/types.h>
#include <sys/int_types.h>
#include <types_posix.h>
#include <unistd.h>
#include <types_off.h>

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
typedef uint32_t size32_t;
typedef uint32_t        caddr32_t;
#if 0
#ifdef _LP64
typedef ino_t		ino64_t;
#else
typedef u_longlong_t	ino64_t;	/* expanded inode type	*/
#endif
#endif

/*
 * return x rounded up to an align boundary
 * eg, P2ROUNDUP(0x1234, 0x100) == 0x1300 (0x13*align)
 * eg, P2ROUNDUP(0x5600, 0x100) == 0x5600 (0x56*align)
 */
#define	P2ROUNDUP(x, align)		(-(-(x) & -(align)))

#if __KERNEL__
#define PAGESIZE        PAGE_SIZE
#else
#define PAGESIZE        (_sysconf(_SC_PAGESIZE))
#endif /*__KERNEL__*/

#ifndef __USE_FILE_OFFSET64
#define pread64		pread
#define pwrite64	pwrite
#endif


#define	SHT_SUNW_dof		0x6ffffff4

#define	EM_AMD64	62		/* AMDs x86-64 architecture */

#define	STV_ELIMINATE	6

#define	SHN_SUNW_IGNORE	0xff3f

#define OBJFS_ROOT      "/system/object"

#define _MAP_NEW        0x80000000      /* users should not need to use this */

#define MISYS_MEMINFO           0x0
struct memcntl_mha {
        uint_t          mha_cmd;        /* command(s) */
        uint_t          mha_flags;
        size_t          mha_pagesize;
};

struct memcntl_mha32 {
        uint_t          mha_cmd;        /* command(s) */
        uint_t          mha_flags;
        size32_t        mha_pagesize;
};

#define MC_HAT_ADVISE   7 

typedef struct meminfo {
        const uint64_t *mi_inaddr;      /* array of input addresses */
        const uint_t *mi_info_req;      /* array of types of info requested */
        uint64_t *mi_outdata;           /* array of results are placed */
        uint_t *mi_validity;            /* array of bitwise result codes */
        int mi_info_count;              /* number of pieces of info requested */
} meminfo_t;
typedef struct meminfo32 {
        caddr32_t mi_inaddr;    /* array of input addresses */
        caddr32_t mi_info_req;  /* array of types of information requested */
        caddr32_t mi_outdata;   /* array of results are placed */
        caddr32_t mi_validity;  /* array of bitwise result codes */
        int32_t mi_info_count;  /* number of pieces of information requested */
} meminfo32_t;

/* sys/socketvar.h */
/*
 *  * Socket versions. Used by the socket library when calling _so_socket().
 *   */
#define SOV_STREAM      0       /* Not a socket - just a stream */
#define SOV_DEFAULT     1       /* Select based on so_default_version */
#define SOV_SOCKSTREAM  2       /* Socket plus streams operations */
#define SOV_SOCKBSD     3       /* Socket with no streams operations */
#define SOV_XPG4_2      4       /* Xnet socket */



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

/* Solaris defined `get' commands (513-768) */
#define	SI_PLATFORM		513	/* return platform identifier */
#define	SI_ISALIST		514	/* return supported isa list */



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


/*
 * Definitions of synchronization types.
 */
#define USYNC_THREAD    0x00            /* private to a process */
#define USYNC_PROCESS   0x01            /* shared by processes */

#define	PF_SUNW_FAILURE	0x00100000	/* mapping absent due to failure */
#define	PN_XNUM		0xffff		/* extended program header index */

/*
 * Definitions for corectl() system call.
 */

/* contents */
#define	CC_CONTENT_STACK	0x0001ULL /* process stack */
#define	CC_CONTENT_HEAP		0x0002ULL /* process heap */
 
/* MAP_SHARED file mappings */
#define	CC_CONTENT_SHFILE	0x0004ULL /* file-backed shared mapping */
#define	CC_CONTENT_SHANON	0x0008ULL /* anonymous shared mapping */
 
/* MAP_PRIVATE file mappings */
#define	CC_CONTENT_TEXT		0x0010ULL /* read/exec file mappings */
#define	CC_CONTENT_DATA		0x0020ULL /* writable file mappings */
#define	CC_CONTENT_RODATA	0x0040ULL /* read-only file mappings */
#define	CC_CONTENT_ANON		0x0080ULL /* anonymous mappings (MAP_ANON) */
 
#define	CC_CONTENT_SHM		0x0100ULL /* System V shared memory */
#define	CC_CONTENT_ISM		0x0200ULL /* intimate shared memory */
#define	CC_CONTENT_DISM		0x0400ULL /* dynamic intimate shared memory */
 
#define	CC_CONTENT_CTF		0x0800ULL /* CTF data */
#define	CC_CONTENT_SYMTAB	0x1000ULL /* symbol table */
 
#define	CC_CONTENT_ALL		0x1fffULL
#define	CC_CONTENT_NONE		0ULL
#define	CC_CONTENT_DEFAULT	(CC_CONTENT_STACK | CC_CONTENT_HEAP | \
CC_CONTENT_ISM | CC_CONTENT_DISM | CC_CONTENT_SHM | \
CC_CONTENT_SHANON | CC_CONTENT_TEXT | CC_CONTENT_DATA | \
CC_CONTENT_RODATA | CC_CONTENT_ANON | CC_CONTENT_CTF | \
CC_CONTENT_SYMTAB)
#define	CC_CONTENT_INVALID	(-1ULL)


/*
 *	system call numbers
 *		syscall(SYS_xxxx, ...)
 */
#define	SYS_fstatat		66
#define	SYS_tasksys		70
#define SYS_rctlsys		74
#define	SYS_statvfs		103
#define	SYS_fstatvfs		104
#define SYS_memcntl 		131
#define SYS_lwp_exit            160
#define SYS_llseek              175
#define SYS_lgrpsys             180
#define	SYS_processor_bind	187
#define	SYS_door		201
#define SYS_setrlimit64         220
#define SYS_getrlimit64         221
#define	SYS_zone		227
#define SYS_getpeername         243
#define SYS_getsockname         244
#define SYS_getsockopt          245
#define SYS_meminfosys          SYS_lgrpsys



/*
 * File share reservation type
 */
typedef struct fshare {
	short	f_access;
 	short	f_deny;
 	int	f_id;
} fshare_t;


/*
 * Commands that refer to flock structures.  The argument types differ between
 * the large and small file environments; therefore, the #defined values must
 * as well.
 * The NBMAND forms are private and should not be used.
 */

#if defined(_LP64) || _FILE_OFFSET_BITS == 32
/* "Native" application compilation environment */
#define	F_ALLOCSP	10	/* Allocate file space */
#define	F_FREESP	11	/* Free file space */
#define	F_SETLK_NBMAND	42	/* private */
#else
/* ILP32 large file application compilation environment version */
#define	F_ALLOCSP	28	/* Alllocate file space */
#define	F_FREESP	27	/* Free file space */
#define	F_SETLK_NBMAND	44	/* private */
#endif /* _LP64 || _FILE_OFFSET_BITS == 32 */

#if 	defined(_LARGEFILE64_SOURCE)

#if !defined(_LP64) || defined(_KERNEL)

/*
 * transitional large file interface version
 * These are only valid in a 32 bit application compiled with large files
 * option, for source compatibility, the 64-bit versions are mapped back
 * to the native versions.
 */
#define	F_ALLOCSP64	28	/* Allocate file space */
#define	F_FREESP64	27	/* Free file space */
#define	F_SETLK64_NBMAND	44	/* private */
#else
#define	F_ALLOCSP64	10	/* Allocate file space */
#define	F_FREESP64	11	/* Free file space */
#define	F_SETLK64_NBMAND	42	/* private */
#endif /* !_LP64 || _KERNEL */

#endif /* _LARGEFILE64_SOURCE */


#define	F_SHARE		40	/* Set a file share reservation */
#define	F_UNSHARE	41	/* Remove a file share reservation */
#define	F_SHARE_NBMAND	43	/* private */

#define	F_BADFD		46	/* Create Poison FD */


/*
 * p_flag codes
 *
 * note that two of these flags, SMSACCT and SSYS, are exported to /proc's
 * psinfo_t.p_flag field.  Historically, all were, but since they are
 * implementation dependant, we only export the ones people have come to
 * rely upon.  Hence, the bit positions of SSYS and SMSACCT should not be
 * altered.
 */
#define	SSYS	   0x00000001	/* system (resident) process */


#define	PS_OBJ_LDSO	((const char *)0x1)	/* the dynamic linker */

#define	Z_OK		0
#define	Z_NO_ZONE		16	/* no such zone */
#define	ZONENAME_MAX		64
#define	GLOBAL_ZONENAME		"global"


#endif
