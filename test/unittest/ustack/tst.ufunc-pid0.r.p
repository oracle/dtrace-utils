#!/usr/bin/gawk -f

# remove trailing blanks, use only one line
{ sub(" *$", ""); print; exit }
