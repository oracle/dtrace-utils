#!/usr/bin/gawk -f

# read data against which we will check tracemem() results
BEGIN {
        n = 0;    # count check results
        m = 0;    # count tracemem() results
}
NF == 2 {
        check[n] = $1; n++;
        check[n] = $2; n++;
        next;
}

# if we have a blank line, report number of bytes compared, if any
NF == 0 && m > 0 { print 8 * m, "bytes" }

# restart counting tracemem() output
NF == 0 { m = 0; next; }

# comparison function
function mycomp(expect, actual) {
        if ( expect != actual )
                print "ERROR: expect", expect, "actual", actual;
}

# tracemem output has two sets of 8 bytes each; reverse bytes for comparison
/:/ {
        mycomp( check[m++], $9$8$7$6$5$4$3$2);
        mycomp( check[m++], $17$16$15$14$13$12$11$10);
}
