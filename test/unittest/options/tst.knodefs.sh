#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2022, 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

$dtrace $dt_flags -xlinkmode=dynamic -xknodefs \
	-Sn 'BEGIN { trace((string)&`linux_banner); exit(0); }' 2>&1 | \
	gawk '/^KREL/ {
		print;
		while (getline == 1) {
			if (NF == 0)
				break;
			print;
			if ($1 == "R_BPF_INSN_64" && $4 == "linux_banner")
				cnt++;
		}
	     }
	     END {
		exit(cnt == 1 ? 0 : 1);
	     }'

exit $?
