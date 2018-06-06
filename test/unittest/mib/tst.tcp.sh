#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2007, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# @@timeout: 10

#
# This script tests that several of the mib:::tcp* probes fire and fire
# with a valid args[0].
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
dtraceout=$tmpdir/dtrace.out.$$
timeout=5
port=2000

if [ -f $dtraceout ]; then
	rm -f $dtraceout
fi

script()
{
	$dtrace $dt_flags -o $dtraceout -s /dev/stdin <<EOF
	mib:::tcpActiveOpens
	{
		opens = args[0];
	}

	mib:::tcpOutDataBytes
	{
		bytes = args[0];
	}

	mib:::tcpOutDataSegs
	{
		segs = args[0];
	}

	profile:::tick-10msec
	/opens && bytes && segs/
	{
		exit(0);
	}

	profile:::tick-1s
	/n++ >= 10/
	{
		exit(1);
	}
EOF
}

server()
{
	perl /dev/stdin /dev/stdout << EOF
	use strict;
	use Socket;

	socket(S, AF_INET, SOCK_STREAM, getprotobyname('tcp'))
	    or die "socket() failed: \$!";

	setsockopt(S, SOL_SOCKET, SO_REUSEADDR, 1)
	    or die "setsockopt() failed: \$!";

	my \$addr = sockaddr_in($port, INADDR_ANY);
	bind(S, \$addr) or die "bind() failed: \$!";
	listen(S, SOMAXCONN) or die "listen() failed: \$!";

	while (1) {
		next unless my \$raddr = accept(SESSION, S);

		while (<SESSION>) {
		}

		close SESSION;
	}
EOF
}

client()
{
	perl /dev/stdin /dev/stdout <<EOF
	use strict;
	use Socket;

	my \$peer = sockaddr_in($port, INADDR_ANY);

	socket(S, AF_INET, SOCK_STREAM, getprotobyname('tcp'))
	    or die "socket() failed: \$!";

	connect(S, \$peer) or die "connect failed: \$!";

	for (my \$i = 0; \$i < 10; \$i++) {
		send(S, "There!", 0) or die "send() failed: \$!";
		sleep (1);
	}
EOF
}

script &
dtrace_pid=$!

#
# Sleep while the above script fires into life. To guard against dtrace dying
# and us sleeping forever we allow 5 secs for this to happen. This should be
# enough for even the slowest systems.
#
while [ ! -f $dtraceout ]; do
	sleep 1
	timeout=$(($timeout-1))
	if [ $timeout -eq 0 ]; then
		echo "dtrace failed to start. Exiting."
		exit 1
	fi
done

server &
server_pid=$!
disown %+
sleep 2
client &
client_pid=$!
disown %+

wait $dtrace_pid
status=$?

kill $server_pid
kill $client_pid

exit $status
