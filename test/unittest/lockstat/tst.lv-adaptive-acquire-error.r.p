#!/usr/bin/gawk -f
NR == 1 { next; }
NR == 2 { print "PROBE", $2, $3, $NF; next; }
/^ *[0-9]+/ { exit; }
{ print; }
