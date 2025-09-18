#!/bin/sh
if ! ${OBJDUMP} -d test/triggers/ustack-tst-basic | grep hlt; then
    echo "did not find hlt instruction in trigger"
    exit 2
fi
exit 0
