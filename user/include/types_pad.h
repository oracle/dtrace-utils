#ifndef TYPES_PAD_H
#define TYPES_PAD_H

/*
 * The {u,}pad64_t types can be used in structures such that those structures
 * may be accessed by code produced by compilation environments which don't
 * support a 64 bit integral datatype.  The intention is not to allow
 * use of these fields in such environments, but to maintain the alignment
 * and offsets of the structure.
 *
 * Similar comments for {u,}pad128_t.
 *
 * Note that these types do NOT generate any stronger alignment constraints
 * than those available in the underlying ABI.  See <sys/isa_defs.h>
 */
#if defined(_INT64_TYPE)
typedef int64_t         pad64_t;
typedef uint64_t        upad64_t;
#else
typedef union {
        double   _d;
        int32_t  _l[2];
} pad64_t;

typedef union {
        double   _d;
        uint32_t _l[2];
} upad64_t;
#endif

typedef union {
        long double     _q;
        int32_t         _l[4];
} pad128_t;

typedef union {
        long double     _q;
        uint32_t        _l[4];
} upad128_t;

typedef longlong_t      offset_t;
typedef u_longlong_t    u_offset_t;
typedef u_longlong_t    len_t;
typedef u_longlong_t    diskaddr_t;
#if (defined(_KERNEL) || defined(_KMEMUSER) || defined(_BOOT))
typedef uint64_t        paddr_t;
#endif


#endif
