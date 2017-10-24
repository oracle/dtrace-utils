#!/bin/bash
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# The existence of this file is a reasonable proxy for
# being on a 32-bit-capable host.
[[ -e test/triggers/libproc-sleeper-32 ]] && exit 0
exit 1
