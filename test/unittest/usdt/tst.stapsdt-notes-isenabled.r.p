#!/usr/bin/gawk -f

# Replace pid in provider name with "$pid" for standardized output.
{ sub(/^test__prov[0-9][0-9]*:/, "test__prov$pid:"); print; }
