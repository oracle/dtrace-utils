#!/usr/bin/gawk -f

NR == 1 { val = base = strtonum("0x"$1); }
NR > 1 { val = strtonum("0x"$1); }
NF > 0 { printf "%x\n", val - base; next; }
{ print; }
