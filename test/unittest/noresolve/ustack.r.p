#!/bin/bash
#
# The ordering of lines is fairly random, due to noise from dead stack
# frames being picked up as real ones.  Just look for various things
# we know should be there, in whatever order.  Also translate *at functions
# into their non-at variants, to help with compatibility with glibcs before
# they started implementing all the non-ats in terms of ats.

sed 's,+0x[0-9a-f]*,,g; s,at$,,' | grep -F -m 2 -e 'readwholedir' -e 'libc.so.6' | sort -u 
