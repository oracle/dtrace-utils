#!/usr/bin/gawk -f
NR == 1 { next; }
NR == 2 { print "PROBE", $2, $3, $NF; next; }
/args\[[0-9]+\]: uint64_t$/ { sub(/:.*$/, ": TYPE-OK"); }
/args\[[0-9]+\]: struct task_struct \*$/ { sub(/:.*$/, ": TYPE-OK"); }
/^ *[0-9]+/ { exit; }
{ print; }
