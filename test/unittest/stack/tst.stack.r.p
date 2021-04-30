#!/usr/bin/gawk -f

/ksys_write/ {
    # check probe
    if ( $1 != "ksys_write:entry" ) {
        print "ERROR: expected fun:prb = ksys_write:entry";
        exit 1;
    }

    # check stack(1)
    getline;
    if (index($1, "`ksys_write+0x") == 0 &&
        match($1, "`ksys_write$") == 0) {
        print "ERROR: expected leaf frame to be ksys_write";
        exit 1;
    }
    FRAME1 = $1;
    getline;
    if (NF > 0) {
        print "ERROR: expected stack(1) to have only one frame";
        exit 1;
    }

    # check stack(2)
    getline;
    if ($1 != FRAME1) {
        print "ERROR: stack(2) leaf frame looks wrong";
        exit 1;
    }
    getline;
    FRAME2 = $1;
    getline;
    if (NF > 0) {
        print "ERROR: expected stack(2) to have only two frames";
        exit 1;
    }

    # check stack(3)
    getline;
    if ($1 != FRAME1) {
        print "ERROR: stack(3) leaf frame looks wrong";
        exit 1;
    }
    getline;
    if ($1 != FRAME2) {
        print "ERROR: stack(3) frame2 looks wrong";
        exit 1;
    }
    getline;
    FRAME3 = $1;
    getline;
    if (NF > 0) {
        print "ERROR: expected stack(3) to have only three frames";
        exit 1;
    }

    # check stack()
    getline;
    if ($1 != FRAME1) {
        print "ERROR: stack() leaf frame looks wrong";
        exit 1;
    }
    getline;
    if ($1 != FRAME2) {
        print "ERROR: stack() frame2 looks wrong";
        exit 1;
    }
    getline;
    if ($1 != FRAME3) {
        print "ERROR: stack() frame3 looks wrong";
        exit 1;
    }
    print "success";
    exit(0);
}
