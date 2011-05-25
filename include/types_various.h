#ifndef TYPES_VARIOUS_H
#define TYPES_VARIOUS_H

#include <sys/types.h>
#include <sys/types32.h>
#include <sys/int_types.h>
#include <unistd.h>

typedef struct {                /* syscall set type */
	unsigned int    word[16];
} sysset_t;

typedef struct {	/* return values from system call */
  long	sys_rval1;	/* primary return value from system call */
  long	sys_rval2;	/* second return value from system call */
} sysret_t;

typedef id_t    taskid_t;
typedef id_t    projid_t;
typedef id_t    poolid_t;
typedef id_t    ctid_t;
typedef unsigned long   psaddr_t;
typedef int psetid_t;
typedef unsigned long	Lmid_t;
typedef enum { B_FALSE, B_TRUE} boolean_t;

typedef pid_t	lwpid_t;

typedef uint32_t priv_chunk_t;

typedef struct iovec iovec_t;

/*
 * Strictly conforming ANSI C environments prior to the 1999
 * revision of the C Standard (ISO/IEC 9899:1999) do not have
 * the long long data type.
 */
#if defined(_LONGLONG_TYPE)
typedef long long               longlong_t;
typedef unsigned long long      u_longlong_t;
#else
/* used to reserve space and generate alignment */
typedef union {
        double  _d;
        int32_t _l[2];
} longlong_t;
typedef union {
        double          _d;
        uint32_t        _l[2];
} u_longlong_t;
#endif  /* defined(_LONGLONG_TYPE) */
typedef u_longlong_t    core_content_t;

typedef longlong_t      offset_t;
typedef u_longlong_t    u_offset_t;
typedef u_longlong_t    len_t;
typedef u_longlong_t    diskaddr_t;
#if (defined(_KERNEL) || defined(_KMEMUSER) || defined(_BOOT))
typedef uint64_t        paddr_t;
#endif

typedef unsigned long long hrtime_t;

/*
 * POSIX Extensions
 */
typedef unsigned char   uchar_t;
typedef unsigned short  ushort_t;
typedef unsigned int    uint_t;
typedef unsigned long   ulong_t;

typedef short           cnt_t;          /* ?<count> type */

#if !defined(_LP64) && defined(__cplusplus)
typedef ulong_t major_t;        /* major part of device number */
typedef ulong_t minor_t;        /* minor part of device number */
#else
typedef uint_t major_t;
typedef uint_t minor_t;
#endif

#ifndef _LARGEFILE64_SOURCE
typedef unsigned long long off64_t;
#endif

#if !defined(_PTRDIFF_T) || __cplusplus >= 199711L
#define _PTRDIFF_T
#if defined(_LP64) || defined(_I32LPx)
typedef long    ptrdiff_t;              /* pointer difference */
#else
typedef int     ptrdiff_t;              /* (historical version) */
#endif
#endif


#ifndef __ino64_t_defined
#ifdef _LP64
typedef ino_t		ino64_t;
#else
typedef u_longlong_t	ino64_t;	/* expanded inode type	*/
#endif
#define __ino64_t_defined
#endif

#ifndef _LARGEFILE64_SOURCE
#ifdef _LP64
typedef	blkcnt_t	blkcnt64_t;
#else
typedef longlong_t	blkcnt64_t;
#endif
#endif

#ifndef __blksize_t_defined
#ifdef _LP64
typedef	int		blksize_t;	/* used for block sizes */
#else
typedef	long		blksize_t;	/* used for block sizes */
#endif
#define __blksize_t_defined
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

#define _MAP_NEW        0x80000000      /* users should not need to use this */

#define MC_HAT_ADVISE   7 


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

#ifndef ABS
#define	ABS(a)		((a) < 0 ? -(a) : (a))
#endif


/*
 * negative signal codes are reserved for future use for user generated
 * signals
 */
#define	SI_FROMUSER(sip)	((sip)->si_code <= 0)
#define	SI_FROMKERNEL(sip)	((sip)->si_code > 0)

/*  indication whether to queue the signal or not */
#define	SI_CANQUEUE(c)	((c) <= SI_QUEUE)

#define _SC_NPROCESSORS_MAX     516     /* maximum # of processors */
#define _SC_CPUID_MAX           517     /* maximum CPU id */


#endif
