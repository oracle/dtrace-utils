#!/bin/sh
# df --output=source is quite new: skip if it doesn't work.
if ! df --output=source >/dev/null 2>&1; then
    exit 1
fi
