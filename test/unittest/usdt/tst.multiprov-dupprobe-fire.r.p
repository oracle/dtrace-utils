#!/bin/sh
sed 's,prov\(.\)[0-9]*,prov\1PID,; s,  *, ,g'
