#!/bin/bash
#
# Compute a variety of offsets needed for monitoring of ptrace()d processes'
# dynamic linker, and write them to the file name in $1. We assume that no
# CFLAGS are provided that change the ABI (because the ABI we rely upon is that
# of the child process, not that of dtrace).
#
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

HEADER=$1
INIT=$2
NATIVE_BITNESS_ONLY=$3
NATIVE_BITNESS=

if [[ -n $NATIVE_BITNESS_ONLY ]]; then
    NATIVE_BITNESS=64
fi

NATIVE_BITNESS_OPTION_SUPPORTED="$(echo 'int main (void) { }' | gcc -x c -o /dev/null -m64 - 2>/dev/null && echo t)"

set -e

# Header file.
cat >$HEADER <<EOF
/* Offsets and sizes for various structures in the dynamic linker.

   This is a generated file. */

EOF
echo '#ifndef _'$(basename $HEADER | tr '[a-z.]' '[A-Z_]')'_' >> $HEADER
echo '#define _'$(basename $HEADER | tr '[a-z.]' '[A-Z_]')'_' >> $HEADER
echo >>$HEADER

for BITNESS in 32 64; do
    BITNESS_ARG="-m${NATIVE_BITNESS:-$BITNESS}"
    if [[ "$BITNESS_ARG" = "-m$NATIVE_BITNESS" ]] &&
       [[ -z $NATIVE_BITNESS_OPTION_SUPPORTED ]]; then
        BITNESS_ARG=
    fi
    sed "s,@BITNESS@,$BITNESS,g" <<'EOF' | \
    $CC -std=gnu99 -o $objdir/mkoffsets $CPPFLAGS $BITNESS_ARG -DBITNESS=$BITNESS \
	${NATIVE_BITNESS:+-DNATIVE_BITNESS=$NATIVE_BITNESS} -DCOMPILE_TIME -D_GNU_SOURCE -x c - >/dev/null
/* Compute the offsets and sizes of various structure fields in the dynamic
   linker needed for debugging.

   This is a generated file. */

#include <stddef.h>
#include <stdio.h>
#include <sys/glibc_internal_link.h>

#if defined(NATIVE_BITNESS) && NATIVE_BITNESS != BITNESS
#define BITNESS_OFFSET(name, s, m) \
	printf("#define " #name "_@BITNESS@_OFFSET\t0\n"); \
	printf("#define " #name "_@BITNESS@_SIZE\t0\n")
#else
#define BITNESS_OFFSET(name, s, m) \
	printf("#define " #name "_@BITNESS@_OFFSET\t%li\n", \
		offsetof(struct s, m)); \
	printf("#define " #name "_@BITNESS@_SIZE\t%li\n", \
		sizeof(((struct s *)0)->m))
#endif

int main(void)
{
	printf("#define UINT_@BITNESS@_SIZE\t%li\n", sizeof(unsigned int));
	printf("#ifndef DL_NNS\n");
	printf("#define DL_NNS\t%li\n", DL_NNS);
	printf("#endif\n");

	BITNESS_OFFSET(R_VERSION, r_debug, r_version);
	BITNESS_OFFSET(R_MAP, r_debug, r_map);
	BITNESS_OFFSET(R_BRK, r_debug, r_brk);
	BITNESS_OFFSET(R_STATE, r_debug, r_state);
	BITNESS_OFFSET(R_LDBASE, r_debug, r_ldbase);

	BITNESS_OFFSET(L_ADDR, internal_link_map, l_addr);
	BITNESS_OFFSET(L_NAME, internal_link_map, l_name);
	BITNESS_OFFSET(L_LD, internal_link_map, l_ld);
	BITNESS_OFFSET(L_NEXT, internal_link_map, l_next);
	BITNESS_OFFSET(L_PREV, internal_link_map, l_prev);
	BITNESS_OFFSET(L_SEARCHLIST, internal_link_map, l_searchlist.r_list);

	BITNESS_OFFSET(G_DEBUG, rtld_global, _dl_ns[0]._ns_debug);
	BITNESS_OFFSET(G_DEBUG_SUBSEQUENT, rtld_global, _dl_ns[1]._ns_debug);
	BITNESS_OFFSET(G_NLOADED, rtld_global, _dl_ns[0]._ns_nloaded);
	BITNESS_OFFSET(G_NLOADED_SUBSEQUENT, rtld_global, _dl_ns[1]._ns_nloaded);
	BITNESS_OFFSET(G_NS_LOADED, rtld_global, _dl_ns[0]._ns_loaded);
	BITNESS_OFFSET(G_NS_LOADED_SUBSEQUENT, rtld_global, _dl_ns[1]._ns_loaded);
	BITNESS_OFFSET(G_DL_NNS, rtld_global, _dl_nns);
	BITNESS_OFFSET(G_DL_LOAD_LOCK, rtld_global, _dl_load_lock.mutex.__data.__count);
}
EOF

    ERR=${PIPESTATUS[0]}
    [[ $ERR -ne 0 ]] && exit $ERR

    $objdir/mkoffsets >> $HEADER
    rm -f $objdir/mkoffsets
    echo >> $HEADER
done

cat >> $HEADER <<'EOF'
#if R_LDBASE_32_OFFSET > R_LDBASE_64_OFFSET
#define R_LAST_OFFSET R_LDBASE_32_OFFSET
#else
#define R_LAST_OFFSET R_LDBASE_64_OFFSET
#endif

#if L_SEARCHLIST_32_OFFSET > L_SEARCHLIST_64_OFFSET
#define L_LAST_OFFSET L_SEARCHLIST_32_OFFSET
#else
#define L_LAST_OFFSET L_SEARCHLIST_64_OFFSET
#endif

/*
 * Index field sizes by native offset.  (Only possible for those structures
 * with a public definition outside glibc_internal_link.h.  Other structure
 * offset computations must be done the ugly way.)
 *
 * This won't cope with unions, but will do for now, and frees us from
 * having to litter the code with ugly R_ and L_ constants.
 */

typedef struct rtld_offsets {
	size_t offset[2];
	size_t size[2];
} rtld_offsets_t;

extern rtld_offsets_t r_debug_offsets[R_LAST_OFFSET+1];
extern rtld_offsets_t link_map_offsets[L_LAST_OFFSET+1];

#endif
EOF

# .c file (initialization).

build_offsets_all()
{
    # Note: this will fail if 32-bit compilation of DTrace is attempted,
    # since it assumes that the native offsets are the 64-bit ones.
    # This is almost certainly not worth fixing.
    for name in $(grep '^#define' $HEADER | grep -v 'LAST_OFFSET' | \
                  grep -o " ${2}"'_.*_64_OFFSET'); do
        name_32_offset="$(echo $name | sed 's,_64,_32,')"
        name_64_size="$(echo $name | sed 's,_OFFSET,_SIZE,')"
        name_32_size="$(echo $name | sed 's,_64_OFFSET,_32_SIZE,')"
    cat >> $INIT <<EOF
	rtld_offsets_t ${name}_build = {{${name_32_offset}, ${name}}, {${name_32_size},${name_64_size}}};
	$1_build[${name}] = ${name}_build;
EOF
    done
}

build_offsets_native_only()
{
    for name in $(grep '^#define' $HEADER | grep -v 'LAST_OFFSET' | \
                  grep -o " ${2}"'_.*_64_OFFSET'); do
        name_64_size="$(echo $name | sed 's,_OFFSET,_SIZE,')"
    cat >> $INIT <<EOF
	rtld_offsets_t ${name}_build = {{0, ${name}}, {0, ${name_64_size}}};
	$1_build[${name}] = ${name}_build;
EOF
    done
}

if [[ -z $NATIVE_BITNESS_ONLY ]]; then
    build_offsets=build_offsets_all
else
    build_offsets=build_offsets_native_only
fi

cat > $INIT <<'EOF'
/* Initialize offsets and sizes for various structures in the dynamic linker.

   This is a generated file. */

#include <sys/compiler.h>
#include <string.h>
#include "rtld_offsets.h"

rtld_offsets_t r_debug_offsets[R_LAST_OFFSET+1];
rtld_offsets_t link_map_offsets[L_LAST_OFFSET+1];
static int initialized;

_dt_constructor_(rtld_offsets_init)
static void rtld_offsets_init(void)
{
	if (initialized)
		return;

	rtld_offsets_t r_debug_offsets_build[R_LAST_OFFSET+1];
	rtld_offsets_t link_map_offsets_build[L_LAST_OFFSET+1];

	memset(r_debug_offsets_build, 0, sizeof(struct rtld_offsets) *
	    (R_LAST_OFFSET+1));

EOF
$build_offsets r_debug_offsets R

cat >> $INIT <<'EOF'
	memcpy((void *)r_debug_offsets, r_debug_offsets_build,
	    sizeof(r_debug_offsets_build));

	memset((void *)link_map_offsets_build, 0, sizeof(struct rtld_offsets) *
	    (L_LAST_OFFSET+1));

EOF
$build_offsets link_map_offsets L
cat >> $INIT <<'EOF'

	memcpy((void *)link_map_offsets, link_map_offsets_build,
	    sizeof(link_map_offsets_build));
	initialized = 1;
}
EOF
