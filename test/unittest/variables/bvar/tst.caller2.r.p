#!/bin/bash

# sed:  remove +0x{ptr} offsets
# awk:  look for the ksys_write frame, then write the next two lines
# uniq:  count unique lines
# awk:  report the counts

sed 's/+0x.*$//' \
| /usr/bin/gawk '/ksys_write/ {getline; print $1; getline; print $1; exit(0)}' \
| uniq -c \
| /usr/bin/gawk '{print $1}'
