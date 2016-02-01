#!/bin/bash
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
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

##
#
# ASSERTION:
# -x evaltime=exec runs before all constructors run and so early that it can
# see syscalls made by the dynamic loader; -x evaltime=preinit runs before all
# constructors run, but not early enough to see the dynamic loader syscalls;
# -x evaltime={postinit|main} runs after constructors for any normal executables.
# 32-bit version.
#
# SECTION: evaltime is not yet documented.
#
##

# @@timeout: 20
# @@xfail: 32-on-64 systrace provider doesn't work yet

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

# We pick on the brk() call made by the dynamic loader.  (This is even made by
# statically linked executables, albeit later, and not visible to preinit.)

for trigger in visible-constructor-32; do
    for stage in exec preinit postinit main; do 
        $dtrace $dt_flags -q -c test/triggers/$trigger -x evaltime=$stage -s /dev/stdin <<EOF
syscall::brk:entry /pid == \$target/ { loader_seen = 1; }
syscall::write*:entry /pid == \$target/ { writes++; }
END { printf("evaltime is $stage, trigger is %s, %i write()s; dynamic loader syscalls %s.\n",
      "$trigger", writes, loader_seen ? "seen" : "not seen"); }
EOF
    done
done

for trigger in visible-constructor-32; do
    $dtrace $dt_flags -q -c test/triggers/$trigger -s /dev/stdin <<EOF
syscall::brk:entry /pid == \$target/ { loader_seen = 1; }
syscall::write*:entry /pid == \$target/ { writes++; }
END { printf("evaltime is default, trigger is %s, %i write()s; dynamic loader syscalls %s.\n",
      "$trigger", writes, loader_seen ? "seen" : "not seen"); }
EOF
done
