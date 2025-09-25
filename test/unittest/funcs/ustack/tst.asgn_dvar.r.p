#!/usr/bin/gawk -f

BEGIN {
    cmd = "uname -rm";
    cmd | getline;
    close(cmd);

    if (/x86_64/) {
	gsub(/\./, " ");
	maj = int($1);
	min = int($2);
	if (maj < 6 || (maj == 6 && min < 11))
	    missing_frame = 1;
    } else
	missing_frame = 1;
}

/myfunc_z/ {
    # check probe
    if ( $1 != "myfunc_z:entry" ) {
        print "ERROR: expected fun:prb = myfunc_z:entry";
        exit(0);
    }

    # check stack(3)
    getline;
    if (index($1, "`myfunc_z+0x") == 0 &&
        match($1, "`myfunc_z$") == 0) {
        print "ERROR: expected leaf frame to be myfunc_z";
        exit(0);
    }
    getline;
    if (NF == 0) {
        print "ERROR: missing second frame";
        exit(0);
    }
    if (index($1, missing_frame ? "`myfunc_x+0x" : "`myfunc_y+0x") == 0 &&
        match($1, missing_frame ? "`myfunc_x$" : "`myfunc_y$") == 0) {
        printf("ERROR: expected leaf frame to be %s\n", missing_frame ? "myfunc_x" : "myfunc_y");
        exit(0);
    }
    getline;
    if (NF == 0) {
        print "ERROR: missing third frame";
        exit(0);
    }
    if (index($1, missing_frame ? "`myfunc_w+0x" : "`myfunc_x+0x") == 0 &&
        match($1, missing_frame ? "`myfunc_w$" : "`myfunc_x$") == 0) {
        printf("ERROR: expected leaf frame to be %s\n", missing_frame ? "myfunc_w" : "myfunc_x");
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
