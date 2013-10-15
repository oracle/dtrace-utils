#!/bin/bash
# The existence of this file is a reasonable proxy for
# being on a 32-bit-capable host.
[[ -e test/triggers/libproc-sleeper-32 ]] && exit 0
exit 1
