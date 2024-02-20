#!/usr/bin/gawk -f

# Report only from 40 to 80.
BEGIN { report = 0 }
$NF == 40 { report = 1 }
$NF == 80 { report = 0 }
report == 1 { print }
