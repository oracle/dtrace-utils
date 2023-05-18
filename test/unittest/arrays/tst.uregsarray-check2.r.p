#!/usr/bin/gawk -f

NF == 0 { next }
$1 == $2 { print "match" }
$1 != $2 { print "ERROR" }
