#!/usr/bin/gawk -f

# Initialize tgt to a dummy value so output without a correct first line does
# not result in trying to replace every character in output with $target.
BEGIN { tgt = "DUMMY"; }

# first line: get the target pid
/^target = [0-9]*$/ { tgt = $3; next }

# other lines: substitute the target pid with "$target"
{ gsub(tgt, "$target"); print }
