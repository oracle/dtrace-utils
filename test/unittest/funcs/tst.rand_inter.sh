#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

dtrace=$1
tmpfile=$tmpdir/tst.rand_inter.$$

# Sanity test of rand().  Do inter-word correlation checks.  That
# is, use lquantize to look at the distribution of 4-bit blocks,
# 2 bits from two consecutive words at arbitrary locations.

$dtrace $dt_flags -q -o $tmpfile -n '
BEGIN
{
	n = 0;
	x = rand();
}
tick-200us
{
	y = rand();

	/* glue 2 bits from x to 2 bits from y */

	xbits = (x >> 30) & 3;
	ybits = (y >> 30) & 3;
	@a30 = lquantize((xbits << 2) | ybits, 0, 16, 1);

	xbits = (x >> 28) & 3;
	ybits = (y >> 28) & 3;
	@a28 = lquantize((xbits << 2) | ybits, 0, 16, 1);

	xbits = (x >> 15) & 3;
	ybits = (y >> 15) & 3;
	@a15 = lquantize((xbits << 2) | ybits, 0, 16, 1);

	xbits = (x >> 02) & 3;
	ybits = (y >> 02) & 3;
	@a02 = lquantize((xbits << 2) | ybits, 0, 16, 1);

	xbits = (x >> 00) & 3;
	ybits = (y >> 00) & 3;
	@a00 = lquantize((xbits << 2) | ybits, 0, 16, 1);

	x = y;
	n++;
}
tick-5sec
{
	printf("number of iterations: %d\n", n);
	exit(0);
}'

if [ $? -ne 0 ]; then
	echo "DTrace error"
	cat $tmpfile
	exit 1
fi

# Now the postprocessing.

awk '
    BEGIN {
        nDistributions = noutlier2 = noutlier3 = noutlier4 = 0;
        nbins = 16;
    }

    # process line: "number of iterations: ..."
    /number of iterations:/ {
        n = int($4);
        if (n < 400) {
            # tick-* can underfire, but require some minimum data
            print "ERROR: insufficient data";
            exit 1;
        }
        avg = n / nbins;     # how many to expect per bin
        std = sqrt(avg);     # standard deviation
        lo2 = avg - 2 * std; # 2 sigma
        hi2 = avg + 2 * std;
        lo3 = avg - 3 * std; # 3 sigma
        hi3 = avg + 3 * std;
        lo4 = avg - 4 * std; # 4 sigma
        hi4 = avg + 4 * std;
        next;
    }

    # skip blank lines
    NF == 0 { next }

    # process line: "value  ------------- Distribution ------------- count"
    /value/ && /Distribution/ && /count/ {
        # process a distribution (lquantize)
        nDistributions++;

        # process line: "< 0 | 0"
        getline;
        if ($1 != "<" || $2 != "0" || $3 != "|" || $4 != "0") {
            print "ERROR: corrupt underflow bin?";
            exit 1;
        }

        # process lines:
        #      0 |@@@                      ...
        #      1 |@@                       ...
        #      2 |@@                       ...
        #      3 |@@@                      ...
        for (v = 0; v < nbins; v++) {
            getline;
            if (int($1) != v || substr($2, 1, 2) != "|@") {
                print "ERROR: corrupt bin?";
                exit 1;
            }
            c = int($3);
            if (c < lo2 || c > hi2) { noutlier2++ };
            if (c < lo3 || c > hi3) { noutlier3++ };
            if (c < lo4 || c > hi4) { noutlier4++ };
        }

        # process line: ">= 16 | 0"
        getline;
        if ($1 != ">=" || int($2) != nbins || $3 != "|" || $4 != "0") {
            print "ERROR: corrupt overflow bin?";
            exit 1;
        }

        next;
    }

    # anything else is unexpected
    {
        print "ERROR: unexpected line";
        print;
        exit 1;
    }

    # reporting
    END {
        if (nDistributions != 5) {
            print "ERROR: found unexpected number of distributions";
            exit 1;
        }

        # due to statistical fluctuations, must guess sensible bounds
        if (noutlier2 > nDistributions * nbins / 2) {
            print "ERROR: found too many bins outside two sigma";
            exit 1;
        }
        if (noutlier3 > 2) {
            print "ERROR: found too many bins outside three sigma";
            exit 1;
        }
        if (noutlier4 > 0) {
            print "ERROR: found too many bins outside four sigma";
            exit 1;
        }
        exit 0;
    }
' $tmpfile > $tmpfile.summary

if [ $? -ne 0 ]; then
	echo "postprocessing error"
	echo "DTrace output"
	cat $tmpfile
	echo "postprocessing output"
	cat $tmpfile.summary
	rm -f $tmpfile $tmpfile.summary
	exit 1
fi

n=`awk '/number of iterations/ { print $4 }' $tmpfile`
echo inter-word correlations tested for $n random numbers
echo success
rm -f $tmpfile $tmpfile.summary
exit 0
