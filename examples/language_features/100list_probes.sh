#!/bin/bash

#  SYNOPSIS
#    ./100list_probes.sh
#
#  DESCRIPTION
#    List all probes.  There are many.

sudo /usr/sbin/dtrace -l | head -10
echo "[...]"
sudo /usr/sbin/dtrace -l | tail -10
