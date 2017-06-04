#!/bin/bash

check_provider lockstat
if (( $? != 0 )); then
        echo "Could not load lockstat provider"
        exit 1
fi
exit 0
