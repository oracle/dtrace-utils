#!/bin/sh

$dtrace -qn 'profile-100ms /pid == 0/ { exit(0) }
             tick-1s { trace("cannot profile pid 0; oversubscribed system?"); exit(2) }'
exit $?
