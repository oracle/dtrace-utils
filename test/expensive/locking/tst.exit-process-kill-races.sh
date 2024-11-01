#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

##
#
# ASSERTION:
# Verify that we do not get segfaults at destruction time due to
# races between the control thread cleanup machinery and the
# dtrace cleanup machinery.
#
# SECTION: dtrace Utility/-c Option
#
##

# This test runs forever until timed out.
# @@timeout: 1500
# @@timeout-success: t

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

# This is a binary search.  We want to find the lowest and highest delays at
# which we flip from process-termination to timeout, then pound on the stuff
# in that range for a while and make sure dtrace doesn't coredump at any point.

# State, one of INITIAL_SEARCH_UPWARDS, SEARCH_{UPWARDS,DOWNWARDS},
# VARIANCE_CHECK_{UPWARDS,DOWNWARDS} or POUND.
state=INITIAL_SEARCH_UPWARDS

# When flipping direction, this is the delay value we flip at, at each end,
# and the last one we flipped at.
flip_low=0
old_flip_low=0
flip_high=
old_flip_high=
# t when a given varying endpoint is known.
vary_low=
vary_high=
# Which state to flip to when hitting one end. (Switches to going the other
# way once the variance at that end is known.)
high_state=VARIANCE_CHECK_HIGH
low_state=VARIANCE_CHECK_LOW
# Ticks down the variance count checks, which are repeated several times.
check_tick=
# Delay interval, keeps halving.
delinterval=
# The sign of the next change to the delay.
sign=+
# The last output of dtrace (used in variance testing)
last_result=
# Used to slice up the space we pound in into ever-smaller pieces.
pound_divisor=

# The delay, in nanosecaons (see test/triggers/delaydie.c)
export delay=1

while :; do
    case $state in
	# The initial upward search is the only unbounded-doubling one.
	INITIAL_SEARCH_UPWARDS) delay=$((delay * 2));;
	SEARCH_*) if [[ -z $delinterval ]]; then
		      delinterval=$(((flip_high - flip_low) / 2));
		  fi
		  delay=$((delay $sign delinterval))
		  delinterval=$((delinterval / 2));;
	VARIANCE_CHECK_*) check_tick=$((check_tick - 1));;
	POUND) if [[ $delay -gt $flip_high ]]; then
		   delinterval=$(((flip_high - flip_low) / pound_divisor))
		   delay=$flip_low
		   # -1 avoids repeating exactly the same intervals often
		   pound_divisor=$(((pound_divisor * 2) - 1))
	       fi
	       delay=$((delay + delinterval));;
    esac

    case $state in
	VARIANCE*) echo "State: $state; delay: $delay; check tick: $check_tick" >&2;;
	SEARCH*|POUND) echo "State: $state; delay: $delay; interval: $delinterval" >&2;;
	*) echo "State: $state; delay: $delay" >&2;;
    esac

    if [[ $state != "POUND" ]]; then
	result="$($dtrace $dt_flags -qc test/triggers/delaydie -n 'tick-1s { printf("timeout\n"); exit(0); }' -n 'syscall::exit_group:entry /pid == $target/ { printf("exit\n"); exit(0); }')"
    else
	set -e
	$dtrace $dt_flags -qc test/triggers/delaydie -n 'tick-1s { printf("timeout\n"); exit(0); }' -n 'syscall::exit_group:entry /pid == $target/ { printf("exit\n"); exit(0); }'
	set +e
    fi

    echo "result: $result" >&2

    case $state in
	*SEARCH_UPWARDS) if [[ $result = "timeout" ]]; then
			     flip_high=$delay
			     check_tick=10
			     unset delinterval
			     state=$high_state
			     sign=-
			 fi;;
	SEARCH_DOWNWARDS) if [[ $result = "exit" ]]; then
			      flip_low=$delay
			      check_tick=10
			      unset delinterval
			      state=$low_state
			      sign=+
			  fi;;
	VARIANCE_CHECK_HIGH) if [[ -n $last_result ]] && [[ $result != $last_result ]]; then
				 vary_high=t
				 # The variance starts somewhere between here
				 # and the last tested possibility. If there was none, we
				 # must search upwards a little longer.
				 if [[ -z $old_flip_high ]]; then
				     state=INITIAL_SEARCH_UPWARDS
				     unset delinterval
				 else
				     flip_high=$old_flip_high
				     delay=$old_flip_high
				     check_tick=0
				     high_state=SEARCH_DOWNWARDS
				 fi
			     fi
			     if [[ state != INITIAL_SEARCH_UPWARDS ]] && [[ $check_tick -eq 0 ]]; then
				 old_flip_high=$flip_high
				 flip_high=$delay
				 if [[ -z $vary_low ]] || [[ -z $vary_high ]]; then
				     state=SEARCH_DOWNWARDS
				     unset delinterval
				 else
				     state=INTO_POUND
				 fi
			     fi;;
	VARIANCE_CHECK_LOW) if [[ -n $last_result ]] && [[ $result != $last_result ]]; then
				vary_low=t
				# The variance starts somewhere between here
				# and the last tested possibility.
				flip_low=$old_flip_low
				delay=$old_flip_low
				check_tick=0
				low_state=SEARCH_UPWARDS
			    fi
			    if [[ $check_tick -eq 0 ]]; then
				old_flip_low=$flip_low
				flip_low=$delay
				if [[ -z $vary_low ]] || [[ -z $vary_high ]]; then
				    state=SEARCH_UPWARDS
				    unset delinterval
				else
				    state=INTO_POUND
				fi
			    fi;;
    esac


    case state in
	SEARCH_*) # If we've got to a zero interval, we know that the critical
		  # region must be a zero-interval range one higher than the
		  # current one (reversing the sign flip done just above): switch
		  # to pounding on it.
		  if [[ -n $delinterval ]] && [[ $delinterval -eq 0 ]]; then
		      flip_low=$((delay $sign -1))
		      flip_high=$flip_low
		      state=INTO_POUND
		  fi;;
    esac

    if [[ $state = INTO_POUND ]]; then
	state=POUND
	pound_divisor=2
	delay=$((flip_high + 1))
    fi
    last_result=$result
done
