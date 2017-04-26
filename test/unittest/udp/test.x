#!/bin/bash
if ! check_provider udp; then
        echo "Could not load udp provider"
        exit 1
fi
if ! perl -MIO::Socket::IP -e 'exit(0);' 2>/dev/null; then
	exit 1
fi
exit 0
