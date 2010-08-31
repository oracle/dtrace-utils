#ifndef TYPES_POSIX_H
#define TYPES_POSIX_H
/*
 * POSIX Extensions
 */
typedef	unsigned char	uchar_t;
typedef	unsigned short	ushort_t;
typedef	unsigned int	uint_t;
typedef	unsigned long	ulong_t;

typedef	char		*caddr_t;	/* ?<core address> type */
typedef	short		cnt_t;		/* ?<count> type */

#if !defined(_PTRDIFF_T) || __cplusplus >= 199711L
#define	_PTRDIFF_T
#if defined(_LP64) || defined(_I32LPx)
typedef	long	ptrdiff_t;		/* pointer difference */
#else
typedef	int	ptrdiff_t;		/* (historical version) */
#endif
#endif

#endif
