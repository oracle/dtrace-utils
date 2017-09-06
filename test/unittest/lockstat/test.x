#!/bin/bash
if ! check_provider lockstat; then
        echo "Could not load lockstat provider"
        exit 1
fi
exit 0
