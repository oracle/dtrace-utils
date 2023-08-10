#!/bin/sh
grep -v '^ *ID' | sed 's,^[0-9]*,PID,; s,prov[0-9]*,prov,g; s,  *, ,g'
