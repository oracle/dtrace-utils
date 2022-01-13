#!/usr/bin/awk -f

{ print; }
/0:( [0-9A-F]{2}){16}  dtrace/ { exit(0); }
END { exit(1); }
