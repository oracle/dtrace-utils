#!/usr/bin/gawk -f

# skip blank lines
NF == 0 { next; }

# expect every line to have 7 fields
NF != 7 { exit(1); }

{
    # check last column
    # (We expect the five probe firings to have different
    # aggregation keys since at least i will be different.)
    if ($7 != 1) { print "last column expected to be 1";
                   exit(1);
                 }

    # we do not care what the other numbers are
    gsub(/ -?[0-9]+/, " number ");

    # ignore the first column (which can be anything)
    # ignore the last column (which we already checked)
    print $2, $3, $4, $5, $6;
}
