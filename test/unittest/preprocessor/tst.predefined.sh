#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Confirm preprocessor pre-definitions.

dtrace=$1

DIRNAME=$tmpdir/predefined.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Arg 1 is macro that we check is defined.

function check_defined() {
	# Add to script: #ifdef is okay, else is ERROR.
	echo '#ifdef' $1                         >> D.d
	echo 'printf("'$1' okay\n");'            >> D.d
	echo '#else'                             >> D.d
	echo 'printf("ERROR!  missing '$1'\n");' >> D.d
	echo '#endif'                            >> D.d

	# Add to check file: expect "okay" message.
	echo $1 okay                             >> chk.txt
}

# Arg 1 is macro whose value we check to be arg 2.

function check_value() {
	# Add to script: print value.
	echo 'printf("'$1'=%x\n", '$1');'        >> D.d

	# Add to check file: expected value.
	echo $1=$2                               >> chk.txt
}

# Arg 1 is macro that we check is not defined.

function check_undef() {
	# Add to script: #ifdef is ERROR, else is okay.
	echo '#ifdef' $1                         >> D.d
	echo 'printf("ERROR!  found '$1'\n");'   >> D.d
	echo '#else'                             >> D.d
	echo 'printf("missing '$1' is okay\n");' >> D.d
	echo '#endif'                            >> D.d

	# Add to check file: expect "okay" message.
	echo missing $1 is okay                  >> chk.txt
}

# Construct version string (major, minor, micro).

read MM mmm uuu <<< `$dtrace -vV | awk '/^This is DTrace / { gsub("\\\.", " "); print $(NF-2), $(NF-1), $NF }'`
vers=`printf "%x" $(($MM << 24 | $mmm << 12 | $uuu))`

# Start setting up the D script.

echo 'BEGIN {' > D.d

# Check for the preprocessor definitions of COMMANDLINE-OPTIONS.

check_defined __linux
check_defined __unix
check_defined __SVR4
if [ `uname -m` == x86_64 ]; then
check_defined __amd64
else
check_undef   __amd64
fi
check_defined __`uname -s`
check_value   __SUNW_D 1
check_value   __SUNW_D_VERSION $vers

# Confirm other preprocessor definitions.

check_defined __SUNW_D_64

# Confirm that __GNUC__ is not present.

check_undef __GNUC__

# Finish setting up the D script.

echo 'exit(0); }' >> D.d
echo              >> chk.txt

# Run the D script.

$dtrace $dt_flags -qCs D.d -o out.txt
if [ $? -ne 0 ]; then
	echo ERROR: DTrace failed
	echo "==== D.d"
	cat        D.d
	echo "==== out.txt"
	cat        out.txt
	exit 1
fi

# Check.

if ! diff -q chk.txt out.txt; then
	echo ERROR output disagrees
	echo === expect ===
	cat chk.txt
	echo === actual ===
	cat out.txt
	echo === diff ===
	diff chk.txt out.txt
	exit 1
fi

# Indicate success.

echo success

exit 0
