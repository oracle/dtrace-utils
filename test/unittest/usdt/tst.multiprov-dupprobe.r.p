#!/bin/sh

# Remove banner.
# Replace numerical values with generic PRID and PID labels.
grep -v '^ *ID' | sed 's,^[0-9][0-9]*,PRID,; s,prov\(.\)[0-9]*,prov\1PID,; s,  *, ,g'
