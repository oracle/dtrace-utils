/*
 * Copyright disclaimed (all content required for interoperability).
 */

#ifndef _SYS_GLIBC_INTERNAL_LINK_H
#define	_SYS_GLIBC_INTERNAL_LINK_H

#include <sys/types.h>
#include <elf.h>
#include <link.h>
#include <pthread.h>
#include <platform.h>

/*
 * This header is derived directly from the GNU C Library's definition.
 *
 * Its members are internal to the dynamic linker and must be kept in synch with
 * the versions there.
 *
 * It is only used at compile time, never at runtime.
 */

#ifndef COMPILE_TIME
#error glibc_internal_link.h is only useful at compile time.
#endif

typedef struct { pthread_mutex_t mutex; } __rtld_lock_recursive_t;

struct internal_link_map;

struct r_scope_elem
{
  /* Array of maps for the scope.  */
  struct internal_link_map **r_list;
  /* Number of entries in the scope.  */
  unsigned int r_nlist;
};

struct internal_link_map
{
  /* These first few members are part of the protocol with the debugger.
     This is the same format used in SVR4.  */

  ElfW(Addr) l_addr;		/* Base address shared object is loaded at.  */
  char *l_name;		/* Absolute file name object was found in.  */
  ElfW(Dyn) *l_ld;		/* Dynamic section of the shared object.  */
  struct link_map *l_next, *l_prev; /* Chain of loaded objects.  */

  /* All following members are internal to the dynamic linker.
     They may change without notice.

     From this point down, the content of this structure is from glibc-2.12 and
     need not change for future glibc releases unless l_searchlist moves out of
     the structure or moves closer to its start:
     libproc/rtld_db.c:find_l_searchlist() wil adapt to most changes that move
     l_searchlist to higher offsets. */

  /* This is an element which is only ever different from a pointer to
     the very same copy of this type for ld.so when it is used in more
     than one namespace.  */
  struct link_map *l_real;

  /* Number of the namespace this link map belongs to.  */
  Lmid_t l_ns;

  void *l_libname;
  /* Indexed pointers to dynamic section.
     [0,DT_NUM) are indexed by the processor-independent tags.
     [DT_NUM,DT_NUM+DT_THISPROCNUM) are indexed by the tag minus DT_LOPROC.
     [DT_NUM+DT_THISPROCNUM,DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM) are
     indexed by DT_VERSIONTAGIDX(tagvalue).
     [DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM,
	DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM) are indexed by
     DT_EXTRATAGIDX(tagvalue).
     [DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM,
	DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM+DT_VALNUM) are
     indexed by DT_VALTAGIDX(tagvalue) and
     [DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM+DT_VALNUM,
	DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM+DT_VALNUM+DT_ADDRNUM)
     are indexed by DT_ADDRTAGIDX(tagvalue), see <elf.h>.  */

  ElfW(Dyn) *l_info[DT_NUM + DT_THISPROCNUM + DT_VERSIONTAGNUM
		      + DT_EXTRANUM + DT_VALNUM + DT_ADDRNUM];
  const ElfW(Phdr) *l_phdr;	/* Pointer to program header table in core.  */
  ElfW(Addr) l_entry;		/* Entry point location.  */
  ElfW(Half) l_phnum;		/* Number of program header entries.  */
  ElfW(Half) l_ldnum;		/* Number of dynamic segment entries.  */

  /* Array of DT_NEEDED dependencies and their dependencies, in
     dependency order for symbol lookup (with and without
     duplicates).  There is no entry before the dependencies have
     been loaded.  */
  struct r_scope_elem l_searchlist;
};

struct rtld_global
{
  /* Don't change the order of the following elements.  'dl_loaded'
     must remain the first element.  Forever.  */

# define DL_NNS 16
  struct link_namespaces
  {
    /* A pointer to the map for the main map.  */
    struct link_map *_ns_loaded;
    /* Number of object in the _dl_loaded list.  */
    unsigned int _ns_nloaded;
    /* Direct pointer to the searchlist of the main object.  */
    void *_ns_main_searchlist;
    /* This is zero at program start to signal that the global scope map is
       allocated by rtld.  Later it keeps the size of the map.  It might be
       reset if in _dl_close if the last global object is removed.  */
    size_t _ns_global_scope_alloc;
    /* Search table for unique objects.  */
    struct unique_sym_table
    {
      __rtld_lock_recursive_t lock;
      void *entries;
      size_t size;
      size_t n_elements;
      void (*free) (void *);
    } _ns_unique_sym_table;
    /* Keep track of changes to each namespace' list.  */
    struct r_debug _ns_debug;
  } _dl_ns[DL_NNS];
  size_t _dl_nns;
  /* Lock protecting concurrent open/close.  */
    __rtld_lock_recursive_t _dl_load_lock;
} rtld_global_t;

#endif	/* _SYS_GLIBC_INTERNAL_LINK_H */
