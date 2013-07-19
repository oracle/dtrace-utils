#!/bin/bash
#
# The ordering of lines is fairly random, due to noise from dead stack
# frames being picked up as real ones.  Just look for various things
# we know should be there, in whatever order.

sed 's,+0x[0-9a-f]*,,g' | grep -F -e 'readwholedir`is_regular_test_file' -e 'libc.so.6`scandir' -e 'readwholedir`main' | sort -u 
