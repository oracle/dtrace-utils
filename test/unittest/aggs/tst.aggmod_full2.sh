#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2018, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# We should get the correct module name for each /proc/kallmodsyms
# address.  Each module should be represented by exactly one name.
#
# SECTION: Aggregations/Aggregations
#
##

dtrace=$1
CC=/usr/bin/gcc
CFLAGS=

DIRNAME="$tmpdir/aggs-aggmod_full2.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# ==================================================
# test.c
#
# This program reads addresses from /proc/kallmodsyms and passes
# (addr,modname) pairs to a static probe, which is used to test
# DTrace.  The program also prints modname values to stdout;  we
# can aggregate them with Linux command-line tools to check DTrace.
# ==================================================

cat > test.c << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/limits.h>
#include <time.h>
#include "prov.h"

/*
 * from dtrace-utils file libdtrace/dt_module.c
 * (or see kernel file scripts/kallsyms.c and KSYM_NAME_LEN)
 */
#define KSYM_NAME_MAX 128

int main(int argc, char **argv) {
	char *line = NULL;
	size_t line_n = 0;
	FILE *fd;
	unsigned int kernel_flag = 0;

	if ((fd = fopen("/proc/kallmodsyms", "r")) == NULL) return 1;
	while ((getline(&line, &line_n, fd)) > 0) {
		long long unsigned addr, size;
		char type;
		char symname[KSYM_NAME_MAX];
		char modname[PATH_MAX] = "vmlinux]";

		/* read the symbol */
		if (sscanf(line, "%llx %llx %c %s [%s",
		    &addr, &size, &type, symname, modname) < 4) {
			printf("ERROR: expected at least 4 fields\n");
			fclose(fd);
			return 1;
		}

		/* see dt_module.c dt_modsym_update() */
		if (type == 'a' || type == 'A')
			continue;
#define KERNEL_FLAG_INIT_SCRATCH 4
		if (strcmp(symname, "__init_scratch_begin") == 0) {
			kernel_flag |= KERNEL_FLAG_INIT_SCRATCH;
			continue;
		} else if (strcmp(symname, "__init_scratch_end") == 0) {
			kernel_flag &= ~ KERNEL_FLAG_INIT_SCRATCH;
			continue;
		} else if (kernel_flag & KERNEL_FLAG_INIT_SCRATCH) {
			continue;
		}
#undef KERNEL_FLAG_INIT_SCRATCH

		/* zero-size symbol might mark the end of a range */
		if (size == 0)
			continue;

		if (strcmp(modname, "bpf]") == 0)
			continue;

		/* we cannot dt_module_load("__builtin__ftrace") */
		if (strcmp(modname, "__builtin__ftrace]") == 0)
			continue;

		/* trim the trailing ']' and print modname to stdout */
		modname[strlen(modname)-1] = '\0';
		printf("%s\n", modname);

		/* pass (addr,modname) to the static probe */
		TEST_PROV_AGGMODTEST(addr, modname);
	}
	free(line);
	fclose(fd);
	return 0;
}
EOF

# ==================================================
# prov.d
#
# The static probe arguments are (addr,modname).
# ==================================================

cat > prov.d << EOF
provider test_prov
{
	probe aggmodtest(uint64_t, char *);
};
EOF

# ==================================================
# build
# ==================================================

$dtrace $dt_flags -h -s prov.d
if [ $? -ne 0 ]; then
	echo "failed to generate header file" >& 2
	exit 1
fi

${CC} ${test_cppflags} ${CFLAGS} -c test.c
if [ $? -ne 0 ]; then
	echo "failed to compile test.c" >& 2
	exit 1
fi

$dtrace $dt_flags -G -s prov.d test.o
if [ $? -ne 0 ]; then
	echo "failed to create DOF" >& 2
	exit 1
fi

${CC} ${test_ldflags} ${CFLAGS} prov.o test.o
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >& 2
	exit 1
fi

# ==================================================
# run dtrace
#
# The D script takes (addr,modname) pairs and aggregates
# (mod(addr),modname).  Since the aggregation key has redundancy,
# we'll be able to check DTrace's aggregation of mod().  Output
# goes to two places:
#
# - output.txt:  DTrace aggregations
#
# - stdout:  the C-program list of modname values, which we
#     aggregate (sort | uniq -c) and format (awk) with Linux tools
#     and save in modules.txt for comparison with output.txt
# ==================================================

# The aggregation keys are:
#   - an 8-byte address
#   - a mod name, which should fit into strsize=48
# With the aggregation # value, that's about 64 bytes per entry.
# If /proc/kall[mod]syms has fewer than 200k symbols we care about,
# all that should fit into an aggsize of 16M.
# If this aggsize becomes a problem, we can do something fancy like
# having the trigger bounce among CPUs.  Then the (per-CPU) aggsize
# could be reduced.
$dtrace $dt_flags -xstrsize=48 -xaggsize=16m -c ./a.out -o output.txt -s /dev/stdin << EOF \
    2> errors.txt | sort | uniq -c | gawk '{print $1, $2}' > modules.txt
test_prov\$target:::
{
	@[mod(arg0), copyinstr(arg1)] = count();
}
EOF
if [[ ! -e output.txt ]]; then
	echo $tst: dtrace failed
	cat errors.txt
	exit 1
fi
if [[ `grep -c "aggregation drops on CPU" errors.txt` -gt 0 ]]; then
	echo
	grep "aggregation drops on CPU" errors.txt
	echo "$tst: might need a larger value of aggsize"
	echo
	exit 1
fi

# ==================================================
# post-process DTrace aggregation data
# - read non-blank lines
# - check that the module names ($1 and $2) agree
# - write results so that they're formatted like modules.txt
#     - number ($3) and module name (either $1 or $2)
#     - sorted by module name
# ==================================================

gawk '
    NF>0 {
        if ($1 != $2) { print "ERROR module name mismatch:", $0 };
        print $3, $1;
    }
' output.txt | sort -k2,2 > output2.txt

if [[ `grep -c "ERROR module name mismatch" output2.txt` -gt 0 ]]; then
	echo "ERROR: DTrace mismatches between mod(addr) and modname"
	echo "  first  column is mod(addr)"
	echo "  second column is modname"
	gawk '/ERROR module name mismatch/ {print $5, $6}' output2.txt
	exit 1
fi

# ==================================================
# check DTrace aggregation results
# ==================================================

if [ `diff output2.txt modules.txt | wc -l` -gt 0 ]; then
	echo "ERROR: mismatch in aggregation data"
	echo "---------- from DTrace"
	cat output2.txt
	echo "---------- from C/Linux reading of /proc/kallmodsyms"
	cat modules.txt

	# dump /proc/kallmodsyms
	# (useful since it changes when test suite ends and unloads modules)
	#cat /proc/kallmodsyms

	exit 1
fi

exit 0

