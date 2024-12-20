#!/usr/bin/gawk -f

# first line: get the target pid
/^target = [0-9]*$/ { tgt = $3; next }

# other lines: substitute the target pid with "$target"
{ gsub(tgt, "$target"); print }
