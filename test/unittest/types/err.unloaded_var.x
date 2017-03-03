#!/bin/bash
# XFAIL if hysdn.ko not found (exit 1)
[[ ! -e /lib/modules/$(uname -r)/kernel/drivers/isdn/hysdn/hysdn.ko ]] && exit 1;
exit 0
