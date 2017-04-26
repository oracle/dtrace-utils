#!/bin/sh
if ! check_provider ip; then
        echo "Could not load ip provider"
        exit 1
fi
exit 0
