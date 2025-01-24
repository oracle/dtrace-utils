#!/usr/bin/gawk -f

# This report has a variable probe ID in it.
{
    sub("for probe ID [0-9][0-9]* .profile", "for probe ID nnn (profile");
    print;
}
