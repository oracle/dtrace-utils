/*
 * Copyright disclaimed (all content required for interoperability).
 */

#ifndef _SYS_GLIBC_INTERNAL_LINK_H
#define	_SYS_GLIBC_INTERNAL_LINK_H

#include <sys/types.h>
#include <elf.h>
#include <link.h>
#include <platform.h>

/*
 * This header is derived directly from the GNU C Library's definition.
 * Some of its members are internal to the dynamic linker and should be kept in
 * synch with the versions there.
 *
 * It is only used at compile time, never at runtime.
 */

#ifndef COMPILE_TIME
#error glibc_internal_link.h is only useful at compile time.
#endif

/* Structure describing a loaded shared object.  The `l_next' and `l_prev'
   members form a chain of all the shared objects loaded at startup.

   These data structures exist in space used by the run-time dynamic linker;
   modifying them may have disastrous results.

   This data structure might change in future, if necessary.  User-level
   programs must avoid defining objects of this type.  */

typedef struct internal_link_map
{
    /* These first few members are part of the protocol with the debugger.
       This is the same format used in SVR4.  */

    ElfW(Addr) l_addr;		/* Base address shared object is loaded at.  */
    char *l_name;		/* Absolute file name object was found in.  */
    ElfW(Dyn) *l_ld;		/* Dynamic section of the shared object.  */
    struct link_map *l_next, *l_prev; /* Chain of loaded objects.  */

    /* All following members are internal to the dynamic linker.
       They may change without notice.  */

    /* This is an element which is only ever different from a pointer to
       the very same copy of this type for ld.so when it is used in more
       than one namespace.  */
    struct link_map *l_real;

    /* Number of the namespace this link map belongs to.  */
    Lmid_t l_ns;
} internal_link_map_t;

#endif	/* _SYS_GLIBC_INTERNAL_LINK_H */
