#!/bin/bash

# Convert pid-dependent /proc pathnames into generic names, and sort output.
sed -e 's|/proc/[0-9][0-9]*|/proc/#|' | sort
