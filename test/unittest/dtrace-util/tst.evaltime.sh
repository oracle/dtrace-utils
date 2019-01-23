#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2019, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# The -x option can be used with evaltime to start tracing at the desired stage.
#
# SECTION: dtrace Utility/-x Option
#
##

# @@timeout: 80

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

OLDDIRNAME=`pwd`
DIRNAME="$tmpdir/dtrace-util-evaltime.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# Handle one case.

do_one_case() {

	# $1 is the trigger binary; $2 is the evaltime option
	echo $1 $2

	# Run the D script.
	$dtrace $dt_flags -q -o tmp.txt -c $OLDDIRNAME/test/triggers/$1 $2 -n '

		/* linker makes syscalls and shows up in the ustack */
		syscall:::entry
		/pid == $target/
		{
			ustack();
		}

		/* the constructor and the main program print messages */
		syscall::write*:entry
		/pid == $target/
		{
			printf("write probe fired; numbytes = %i\n", arg2);
		}

		/* workaround since "dtrace -c" with ustack() can hang */
		syscall::exit_group:entry
		/pid == $target/
		{
			exit(0);
		}
	'
	if [ $? -ne 0 ]; then
		echo "DTrace error"
		exit 1
	fi

	# Look for signs of the dynamic linker.
	if grep -q "_dl_start" tmp.txt; then
		echo Saw dynamic linker.
	else
		echo Did not see dynamic linker.
	fi

	# Look for printf() messages.
	grep "write probe fired; numbytes = " tmp.txt

	rm -f tmp.txt
}

# Loop over the cases.

for trigger in \
  visible-constructor \
  visible-constructor-static-unstripped \
  visible-constructor-static \
; do

	for evaltime in \
	  "-x evaltime=exec" \
	  "-x evaltime=preinit" \
	  "-x evaltime=postinit" \
	  "-x evaltime=main" \
	  " "\
	; do

		do_one_case $trigger "$evaltime"

	done
done

exit 0
