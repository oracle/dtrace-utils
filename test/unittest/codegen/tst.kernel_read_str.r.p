#!/usr/bin/awk -f

{ print; }
/^Linux version / { ok = 1; exit(0); }
END { exit(ok ? 0 : 1); }
