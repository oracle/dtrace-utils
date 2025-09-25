#!/usr/bin/gawk -f

/hrtimer_nanosleep/ {
    # check probe
    if ( $1 != "hrtimer_nanosleep:entry" ) {
        print "ERROR: expected fun:prb = hrtimer_nanosleep:entry";
        exit(0);
    }

    # check stack(3)
    getline;
    if (index($1, "`hrtimer_nanosleep+0x") == 0 &&
        match($1, "`hrtimer_nanosleep$") == 0) {
        print "ERROR: expected leaf frame to be hrtimer_nanosleep";
        exit(0);
    }
    getline;
    if (NF == 0) {
        print "ERROR: missing second frame";
        exit(0);
    }
    getline;
    if (NF == 0) {
        print "ERROR: missing third frame";
        exit(0);
    }
    getline;
    if (NF > 0) {
        print "ERROR: expected stack(3) to have only three frames";
        exit(0);
    }

    print "success";
    exit(0);
}
