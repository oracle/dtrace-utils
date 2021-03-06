#!/bin/bash
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# XFAIL if hysdn.ko not found (exit 1)
[[ ! -e /lib/modules/$(uname -r)/kernel/drivers/isdn/hysdn/hysdn.ko ]] &&
[[ ! -e /lib/modules/$(uname -r)/kernel/vmlinux.ctfa ]] && exit 1;
exit 0
