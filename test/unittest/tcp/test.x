#!/bin/bash

check_provider tcp
if (( $? != 0 )); then
        echo "Could not load tcp provider"
        exit 1
fi

if ! perl -MIO::Socket::IP -e 'exit(0);' 2>/dev/null; then
	exit 1
fi
exit 0
