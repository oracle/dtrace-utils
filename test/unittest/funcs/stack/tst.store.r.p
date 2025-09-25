#!/usr/bin/gawk -f

/hrtimer_nanosleep/ {
    # check probe
    if ( $1 != "hrtimer_nanosleep:entry" ) {
        print "ERROR: expected fun:prb = hrtimer_nanosleep:entry";
        exit(0);
    }

    # collect 3 stack frames
    for (i = 0; i < 3; i++) {
        getline;
        if (NF == 0) {
            print "ERROR: missing stack(3) frame addrs["i"]";
            exit(0);
        }
        stack[i] = $0;
    }
    getline;
    if (NF != 0) {
        print "ERROR: too many stack frames (first stack(3))";
        exit(0);
    }

    # expect 2 stack frames
    for (i = 0; i < 2; i++) {
        getline;
        if (NF == 0) {
            print "ERROR: missing stack(2) frame addrs["i"]";
            exit(0);
        }
        if (stack[i] != $0) {
            print "ERROR: wrong stack(2) frame addrs["i"]";
            exit(0);
        }
    }
    getline;
    if (NF != 0) {
        print "ERROR: too many stack frames (stack(2))";
        exit(0);
    }

    # expect 3 stack frames
    for (i = 0; i < 3; i++) {
        getline;
        if (NF == 0) {
            print "ERROR: missing stack(3) frame addrs["i"]";
            exit(0);
        }
        if (stack[i] != $0) {
            print "ERROR: wrong stack(3) frame addrs["i"]";
            exit(0);
        }
    }
    getline;
    if (NF != 0) {
        print "ERROR: too many stack frames (second stack(3))";
        exit(0);
    }

    print "success";
    exit(0);
}
