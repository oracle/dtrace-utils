#!/bin/bash

if [[ ! -f /var/run/syslogd.pid ]] ||
   [[ ! -d "/proc/$(cat /var/run/syslogd.pid)" ]] ||
   grep -qF '(deleted)' "/proc/$(cat /var/run/syslogd.pid)/maps"; then
    exit 1
fi

exit 0
