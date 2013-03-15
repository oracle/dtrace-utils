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
