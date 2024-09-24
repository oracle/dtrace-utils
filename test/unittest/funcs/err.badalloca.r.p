#!/usr/bin/awk -f

# This report has a variable probe ID in it.
{
    sub("for probe ID [0-9]* .profile", "for probe ID NNN (profile");
    print;
}
