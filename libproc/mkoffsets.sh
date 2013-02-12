#!/bin/bash
#
# Compute a variety of offsets needed for monitoring of ptrace()d processes'
# dynamic linker, and write them to the file name in $1. We assume that no
# CFLAGS are provided that change the ABI (because the ABI we rely upon is that
# of the child process, not that of dtrace).
#
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#
#
# Copyright 2013 Oracle, Inc.  All rights reserved.
# Use is subject to license terms.
#

HEADER=$1
INIT=$2

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
    sed "s,@BITNESS@,$BITNESS,g" <<'EOF' | \
    $CC -std=gnu99 -o $objdir/mkoffsets $CPPFLAGS -m$BITNESS -DCOMPILE_TIME \
	-D_GNU_SOURCE -x c - >/dev/null
/* Compute the offsets and sizes of various structure fields in the dynamic
   linker needed for debugging.

   This is a generated file. */

#include <stddef.h>
#include <stdio.h>
#include <sys/glibc_internal_link.h>

#define BITNESS_OFFSET(name, s, m) \
	printf("#define " #name "_@BITNESS@_OFFSET\t%li\n", \
		offsetof(struct s, m)); \
	printf("#define " #name "_@BITNESS@_SIZE\t%li\n", \
		sizeof(((struct s *)0)->m))

int main(void)
{
	BITNESS_OFFSET(R_VERSION, r_debug, r_version);
	BITNESS_OFFSET(R_MAP, r_debug, r_map);
	BITNESS_OFFSET(R_BRK, r_debug, r_brk);
	BITNESS_OFFSET(R_STATE, r_debug, r_state);
	BITNESS_OFFSET(R_LDBASE, r_debug, r_ldbase);

	BITNESS_OFFSET(L_ADDR, internal_link_map, l_addr);
	BITNESS_OFFSET(L_NAME, internal_link_map, l_name);
	BITNESS_OFFSET(L_LD, internal_link_map, l_ld);
	BITNESS_OFFSET(L_NEXT, internal_link_map, l_next);
	BITNESS_OFFSET(L_LMID, internal_link_map, l_ns);
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

#if L_LMID_32_OFFSET > L_LMID_64_OFFSET
#define L_LAST_OFFSET L_LMID_32_OFFSET
#else
#define L_LAST_OFFSET L_LMID_64_OFFSET
#endif

/*
 * Index field sizes by native offset.
 *
 * This won't cope with unions, but will do for now, and frees us from
 * having to litter the code with ugly R_ and L_ constants.
 */

typedef struct rtld_offsets {
	size_t offset[2];
	size_t size[2];
} rtld_offsets_t;

extern const rtld_offsets_t r_debug_offsets[R_LAST_OFFSET+1];
extern const rtld_offsets_t link_map_offsets[L_LAST_OFFSET+1];

#endif
EOF

# .c file (initialization).

build_offsets()
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

cat > $INIT <<'EOF'
/* Initialize offsets and sizes for various structures in the dynamic linker.

   This is a generated file. */

#include <sys/compiler.h>
#include <string.h>
#include "rtld_offsets.h"

const rtld_offsets_t r_debug_offsets[R_LAST_OFFSET+1];
const rtld_offsets_t link_map_offsets[L_LAST_OFFSET+1];
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
build_offsets r_debug_offsets R

cat >> $INIT <<'EOF'
	memcpy((void *) r_debug_offsets, r_debug_offsets_build,
	    sizeof (r_debug_offsets_build));

	memset((void *) link_map_offsets_build, 0, sizeof(struct rtld_offsets) *
	    (L_LAST_OFFSET+1));

EOF
build_offsets link_map_offsets L
cat >> $INIT <<'EOF'

	memcpy((void *) link_map_offsets, link_map_offsets_build,
	    sizeof (link_map_offsets_build));
	initialized = 1;
}
EOF
