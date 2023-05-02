#!/usr/bin/awk -f

# Some Linux kernel versions leave garbage at the end of the string.
{ sub(/( [0-9A-F]{2}){9}  /, " 00 00 00 00 00 00 00 00 00  "); }
{ sub(/  dtrace\..{9}/, "  dtrace.........."); }
{ print; }
