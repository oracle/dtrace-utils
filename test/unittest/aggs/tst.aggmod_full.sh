#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# We should be able to get the module name (sometimes "vmlinux") for each
# address listed in /proc/kallmodsyms.  Each module should be represented
# by exactly one name.
#
# SECTION: Aggregations/Aggregations
#
##

# Big batches of addresses are tested at once in order to
#   - manage run times
#   - check that each module appears with a single name
# This means that it can be hard to pinpoint which addresses are causing
# problems.
#
# Another diagnosibility problem is that the test suite loads and unloads
# modules (see dtrace-user/test/modules).  So reproducibility of a problem
# changes once the test suite finishes.
#
# To get more precise and useful diagnostic information, set debug=1.
# Run time will increase with the number of errors that need to be isolated.
# So you may have to increase the timeout value to see all errors.
#
# This test takes ages and does not suffer the erraticness problems of
# other tests in this directory.  Do not run it more than once.
#
# @@reinvoke-failure: 0
#
# This script will read its own timeout, listed here:
#
# @@timeout: 120
debug=0
#debug=1

dtrace=$1

# progress variables (read the timeout listed above)
# (use "@''@" instead of "@@" so that runtest.sh will not see this line)

timeout=`awk '/^# @''@timeout: [0-9]/ {print $NF}' $0 | head -1`
progress_stop_time=$((`date +%s` + $timeout - 5))
progress_done=0
progress_goal=0

# The temporary directory will be filled with different files:
# - Initially, there will be files named "[$modname]"
#   (including those literal square brackets "[]"),
#   each file contained the addresses corresponding
#   to the module named.
# - Next, these files are replaced by others named "$modname@XXXX"
#   that contain the addresses as parts of D commands.
#   While there is only one "[$modname]" file per module,
#   there can be multiple "$modname@XXXX" files per module
#   to accommodate D program-size limits or, if debug!=0, to
#   improve diagnosibility.  Once a "$modname@XXXX" file has
#   been used, it is deleted.
# - Temporary files like "D.d" and "D.out" come and go.
# The directory will end up empty when all files have been processed.

DIRNAME="$tmpdir/tst.aggmod_full.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

# extract symbol addresses from /proc/kallmodsysms
# - dump them into files named by module name (5th field)
#   . use module name "[vmlinux]" if the field is missing
# - skip if:
#   . size (2nd field) is 0 (use this as the indicator to skip)
#   . type (3rd field) is "a"
#   . type (3rd field) is "A"
# - can add special cases based on symbol name (4th field)
awk '{
if (NF == 4) {$5 = "[vmlinux]"};
if ($3 == "a") {$2 = 0};
if ($3 == "A") {$2 = 0};
if ($2 != 0) { print $1 >> $5 }
}' /proc/kallmodsyms

# process the files whose names start with a '['

for x in \[*; do
	# strip off []
	modname=`echo $x | sed 's/\[//' | sed 's/\]//'`

	# shuffle the address list
	# format each address as part of a D command
	# split the list into large chunks named $modname@XXXX to avoid
	#     dtrace: failed to enable $modname.d:
	#       DIF program exceeds maximum program size
	shuf $x \
	| awk '{printf "@[mod(0x%s)] = count();\n", $1;}' \
	| split --lines=600 - $modname@

	# increment progress goal by the number of addresses
	progress_goal=$(($progress_goal + `wc --lines < $x`))

	# remove the "[$modname]" file
	rm -f $x
done

# function to handle error reporting

report_error() {
	echo ERROR: $1
	cat D.out
	echo "D commands:"
	cat $cmds
	rm $cmds D.out D.d
	nerrors=$(($nerrors + 1))
}

# process the files whose names are of the form $modname@XXXX

nerrors=0
while [[ $(find . -name "*@*" -print -quit | wc -l) -gt 0 ]]; do
	# pick a ./$modname@XXXX file in the directory
	cmds=`find . -name "*@*" -print -quit`
	ncmds=`cat $cmds | wc --lines`

	# progress update
	if [[ `date +%s` -ge $progress_stop_time ]]; then
		break
	fi
	progress_done=$(($progress_done + $ncmds))

	# extract the module name (strip leading ./ and trailing @XXXX)
	modname=`echo $cmds | sed 's/^\.\///' | sed 's/@.*$//'`

	# form a proper D script
	echo "BEGIN {" > D.d
	cat $cmds >> D.d
	echo "}" >> D.d

	# run D script
	$dtrace -q $dt_flags -s D.d -c echo | awk 'NF>0' >& D.out
	status=$?
	if [[ "$status" -ne 0 ]]; then
		report_error "$tst: dtrace failed"
		exit $status
	fi

	# if debug mode and >1 module reported, break the file apart
	if [[ $debug -ne 0 ]] && [[ `cat D.out | wc --lines` -gt 1 ]]; then

		# actually, if only one address, just report and move on
		if [[ $ncmds -lt 2 ]]; then
			report_error "output does not have 1 line"
			continue
		fi

		# divide ncmds by 2 (rounding up)
		nlines=$(((ncmds + 1) / 2))

		# split file to new $modname@XXXX files, using D.* temp files
		split --lines=$nlines $cmds D.
		mv D.aa `mktemp $modname@XXXX`
		mv D.ab `mktemp $modname@XXXX`
		progress_done=$(($progress_done - $ncmds)) # progress setback

		# clean up and continue
		rm $cmds D.out D.d
		echo info: $ncmds addresses in $modname split
		continue
	fi

	# confirm that there is exactly one line
	if [[ $(cat D.out | wc --lines) -ne 1 ]]; then
		report_error "output does not have 1 line"
		continue
	fi

	# sanity check the result file
	if [[ $(cat D.out | wc --words) -ne 2 ]]; then
		report_error "output does not have 2 words"
		continue
	fi

	# extract the data
	check_modname=`awk '{print $1}' D.out`
	check_count=`awk '{print $2}' D.out`

	# special case
	[[ $modname == "ctf" ]] && modname="shared_ctf"

	# check the data
	if [[ $check_modname != $modname ]]; then
		report_error "improper modname;  $check_modname vs $modname"
		continue
	fi
	if [[ $check_count -ne $ncmds ]]; then
		report_error "improper count;  $check_count vs $ncmds"
		continue
	fi

	# all is fine; report and clean up
	if [[ $debug -ne 0 ]]; then
		echo info: $ncmds addresses in $modname retired
	fi
	rm -f $cmds D.out D.d
done

if [[ $nerrors -gt 0 ]]; then
	echo "ERROR: $nerrors errors"
	if [[ $debug -ne 0 ]]; then
		echo "==================== /proc/kallmodsyms"
		cat /proc/kallmodsyms
		echo "===================="
	fi
	exit 1
fi

# It is not necessary to check every last symbol in /proc/kallmodsyms:
# if the timeout will soon be reached, just report how many we checked.
# But do insist that some minimum number have been checked.

progress_done_min=5000
if [[ $progress_done -lt $progress_done_min ]]; then
	echo "ERROR: only $progress_done symbols checked"
	echo "ERROR:   expect minimum of $progress_done_min"
	echo "ERROR: either:"
	echo "ERROR:   - timeout is set too low"
	echo "ERROR:   - test is running anomalously slowly"
	exit 1
fi

echo $progress_done $progress_goal | awk '
{printf "SUCCESS: tested %d of %d addresses = %.1f%%\n", $1, $2, 100. * $1 / $2;}'
exit 0

