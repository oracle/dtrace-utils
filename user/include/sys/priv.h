#ifndef _SYS_PRIV_H
#define _SYS_PRIV_H
 
typedef struct priv_impl_info {
	uint32_t	priv_headersize;	/* sizeof (priv_impl_info) */
	uint32_t	priv_flags;		/* additional flags */
	uint32_t	priv_nsets;		/* number of priv sets */
	uint32_t	priv_setsize;		/* size in priv_chunk_t */
	uint32_t	priv_max;		/* highest actual valid priv */
	uint32_t	priv_infosize;		/* Per proc. additional info */
	uint32_t	priv_globalinfosize;	/* Per system info */
} priv_impl_info_t;

 

#endif
