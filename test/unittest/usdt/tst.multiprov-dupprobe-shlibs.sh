#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

#
# Try multiple shared libraries with multiple providers,
# all with duplicate probe names but with different arguments.
#
# @@nosort

dtrace=$1

DIRNAME="$tmpdir/usdt-multiprov-dupprobe-shlibs.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

#
# Set up the source files.
#

cat > provliba.d <<EOF
provider provliba {
	probe first(int, char *);
	probe second(int);
	probe third(char *a, int b) : (int b, char *a);
};
EOF

cat > provlibb.d <<EOF
provider provlibb {
	probe first(int);
	probe second(char *a, int b) : (int b, char *a);
	probe third(int, char *);
};
EOF

cat > provmain.d <<EOF
provider provmain {
	probe first();
	probe second(int, int, int);
	probe third(char *, int);
};
EOF

cat > liba.c <<EOF
#include "provliba.h"

void
libafunc(void)
{
	PROVLIBA_THIRD("abc", 123);
	PROVLIBA_SECOND(456);
	PROVLIBA_FIRST(789, "def");
}
EOF

cat > libb.c <<EOF
#include "provlibb.h"

void
libbfunc(void)
{
	PROVLIBB_THIRD(12, "AB");
	PROVLIBB_SECOND("CD", 34);
	PROVLIBB_FIRST(56);
}
EOF

cat > main.c <<EOF
#include "provmain.h"
void libafunc(void);
void libbfunc(void);

int
main(int c, char **v)
{
	PROVMAIN_THIRD("abcdef", 1234);
	libafunc();
	PROVMAIN_SECOND(56, 78, 90);
	libbfunc();
	PROVMAIN_FIRST();

	return 0;
}
EOF

#
# Build.
#

for x in liba libb main; do
	$dtrace $dt_flags -h -s prov$x.d
	if [ $? -ne 0 ]; then
		echo failed to generate header file from prov$x.d >&2
		exit 1
	fi
	$CC $test_cppflags -fPIC -c $x.c
	if [ $? -ne 0 ]; then
		echo failed to compile test $x.c >&2
		exit 1
	fi
	$dtrace $dt_flags -G -s prov$x.d $x.o
	if [ $? -ne 0 ]; then
		echo failed to create DOF for $x.o >&2
		exit 1
	fi
done

for x in liba libb; do
	$CC $test_ldflags -shared -o $x.so $x.o prov$x.o -lc
	if [ $? -ne 0 ]; then
		echo failed to build $x.so >&2
		exit 1
	fi
done

LD_RUN_PATH=$PWD $CC $test_ldflags -o main main.o provmain.o -L. -la -lb
if [ $? -ne 0 ]; then
	echo failed to build main >&2
	exit 1
fi

#
# Run DTrace.
#

$dtrace $dt_flags -c ./main -qn '
/* report the target pid for post processing */
BEGIN
{
	printf("target = %d\n", $target);
}

/* have all USDT probes report their fully qualified probe names */
prov*:::
{
	printf("%s:%s:%s:%s\n", probeprov,
				probemod,
				probefunc,
				probename);
}

/* use different clauses to report args for different probes */
provmain$target:::first
{
	printf("%s:::%s\n", probeprov, probename);
}
provliba$target:::second,
provlibb$target:::first
{
	printf("%s:::%s %d\n", probeprov, probename, arg0);
}
provliba$target:::first,
provliba$target:::third,
provlibb$target:::second,
provlibb$target:::third
{
	printf("%s:::%s %d %s\n", probeprov, probename, arg0, stringof(arg1));
}
provmain$target:::third
{
	printf("%s:::%s %s %d\n", probeprov, probename, stringof(arg0), arg1);
}
provmain$target:::second
{
	printf("%s:::%s %d %d %d\n", probeprov, probename, arg0, arg1, arg2);
}
'
if [ $? -ne 0 ]; then
	echo ERROR: dtrace
	exit 1
fi

echo success
exit 0
