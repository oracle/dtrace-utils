#!/usr/bin/gawk -f

# remove trailing blanks
{ sub(" *$", ""); print }
