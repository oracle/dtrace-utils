#!/usr/bin/gawk -f

/myfunc_z/ {
    # check probe
    if ( $1 != "myfunc_z:entry" ) {
        print "ERROR: expected fun:prb = myfunc_z:entry";
        exit(0);
    }

    # collect 5 stack frames
    for (i = 0; i < 5; i++) {
        getline;
        if (NF == 0) {
            print "ERROR: missing stack(5) frame addrs["i"]";
            exit(0);
        }
        stack[i] = $0;
    }
    getline;
    if (NF != 0) {
        print "ERROR: too many stack frames (first stack(5))";
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

    # expect 5 stack frames
    for (i = 0; i < 5; i++) {
        getline;
        if (NF == 0) {
            print "ERROR: missing stack(5) frame addrs["i"]";
            exit(0);
        }
        if (stack[i] != $0) {
            print "ERROR: wrong stack(5) frame addrs["i"]";
            exit(0);
        }
    }
    getline;
    if (NF != 0) {
        print "ERROR: too many stack frames (second stack(5))";
        exit(0);
    }

    print "success";
    exit(0);
}
