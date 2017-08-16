#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
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
