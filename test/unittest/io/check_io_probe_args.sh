#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

#
# @@skip: not used directly by the test harness; called by other scripts
#

infile=$1
retval=0

echo check_io_probe_args $infile

#
# Start with some basic checks on the io probe args.
#

gawk '
BEGIN {
    err = 0;    # set to 1 if we encounter any errors
    nrecs = 0;
}

NF == 0 { next }      # skip empty lines

NF != 23 { err = 1; print "  ERROR (" NR "): garbled input: " $0; next }

{
    nrecs++;

    myprobeprov = $1
    myprobemod = $2
    myprobefunc = $3
    myprobename = $4
    myarg2 = $5
    myb_flags = $6
    myb_bcount = $7
    myb_bufsize = $8
    myb_addr = $9
    myb_resid = $10
    myb_error = $11
    myb_lblkno = $12
    myb_blkno = $13
    myb_iodone = $14
    myb_edev = $15
    myb_major = $16
    myb_minor = $17
    mydev_major = $18
    mydev_minor = $19
    mydev_instance = $20
    mydev_name = $21
    mydev_statname = $22
    mydev_pathname = $23

    # Check probe description.

    if (myprobeprov != "io:") { err = 1; print "  ERROR (" NR "): provider is not io, got", myprobeprov }
    if (myprobemod != "vmlinux:") { err = 1; print "  ERROR (" NR "): module is not vmlinux, got", myprobemod }
    if (myprobefunc != ":") { err = 1; print "  ERROR (" NR "): function is not blank, got", myprobefunc }
    if (myprobename != "wait-start" &&
        myprobename != "wait-done" &&
        myprobename != "start" &&
        myprobename != "done") { err = 1; print "  ERROR (" NR "): name is unrecognized", myprobename }

    # Check that args[2] is 0.
    if (myarg2 != 0) { err = 1; print "  ERROR (" NR "): args[2] should be 0, got", myarg2 }

    # Check for a legal set of flags.
    {
        B_PAGEIO = 0x000010;
        B_PHYS   = 0x000020;
        B_READ   = 0x000040;
        B_WRITE  = 0x000100;
        B_ASYNC  = 0x000400;
        tmp = strtonum("0x"myb_flags);

        # B_ASYNC may be set.
        if (and(tmp, B_ASYNC) != 0) tmp -= B_ASYNC;

        # B_WRITE or else B_READ must be set.
        nflags = 0;
        if (and(tmp, B_WRITE) != 0) {
            tmp -= B_WRITE;
            nflags++;
        }
        if (and(tmp, B_READ) != 0) {
            tmp -= B_READ;
            nflags++;
        }
        if (nflags != 1) {
            printf "flags %x must be read or else write\n", myb_flags;
            err = 1;
        }

        # B_PAGEIO or else B_PHYS must be set.
        nflags = 0;
        if (and(tmp, B_PAGEIO) != 0) {
            tmp -= B_PAGEIO;
            nflags++;
        }
        if (and(tmp, B_PHYS) != 0) {
            tmp -= B_PHYS;
            nflags++;
        }
        if (nflags != 1) {
            printf "flags %x must be pageio or else phys\n", myb_flags;
            err = 1;
        }

        # Check for any other flags.
        if (tmp != 0) {
            printf "flags %x has some expected flags %x set\n", myb_flags, tmp;
            err = 1;
        }
    }

    # FIXME: can we add a check for myb_bcount?

    if (myb_bufsize != myb_bcount) { err = 1; print "  ERROR (" NR "): bcount and bufsize do not match", myb_bcount, myb_bufsize }

    if (myb_addr != "0") { err = 1; print "  ERROR (" NR "): b_addr is not 0:", b_addr }
    if (myb_resid != "0") { err = 1; print "  ERROR (" NR "): b_resid is not 0:", b_resid }
    if (myb_error != "0") { err = 1; print "  ERROR (" NR "): b_error is not 0:", b_error }

    # FIXME: can we add a check for myb_lblkno?

    if (myb_blkno != myb_lblkno) { err = 1; print "  ERROR (" NR "): lblkno and blkno do not match", myb_lblkno, myb_blkno }

    # FIXME: can we add a check for myb_iodone?
    # FIXME: can we add a check for myb_edev?

    if ( myb_major != rshift(myb_edev, 20)) { err = 1; print "  ERROR (" NR "): b_major inconsistent with edev", myb_major, myb_edev }
    if ( myb_minor != and(myb_edev, 0xfffff)) { err = 1; print "  ERROR (" NR "): b_minor inconsistent with edev", myb_minor, myb_edev }

    if (mydev_major != myb_major) { err = 1; print "  ERROR (" NR "): b_major and dev_major do not match", myb_major, mydev_major }
    if (mydev_minor != myb_minor) { err = 1; print "  ERROR (" NR "): b_minor and dev_minor do not match", myb_minor, mydev_minor }

    if (mydev_instance != 0) { err = 1; print "  ERROR (" NR "): dev_instance is not 0", mydev_instance }

    # FIXME: can we add a check for mydev_name?
    # FIXME: can we add a check for mydev_statname?
    # FIXME: can we add a check for mydev_pathname?
}
END {
    if (nrecs == 0) { err = 1; print "  ERROR: no records found" }
    exit(err);
}
' $infile
if [ $? -ne 0 ]; then
    retval=1
    cat $infile
    exit $retval
fi

#
# Check that all iodone PCs are 0 or else correspond to end*io functions.
#

if [ $UID -ne 0 ]; then
    echo skipping iodone check since must be root to read PCs in kallsyms
    retval=1
else
    for pc in `gawk 'NF == 23 { print $14 }' $infile | grep -wv 0 | sort | uniq`; do
        gawk '$1 == "'$pc'" && /end.*io/ { found = 1; exit }
            END { exit(found) }' /proc/kallsyms
        if [ $? -eq 0 ]; then
            echo "  ERROR:", $pc, " is not an end-io function"
            grep $pc /proc/kallsyms
            retval=1
        fi
    done
fi

#
# For each statname, check that the reported major/minor numbers agree with "ls -l".
#

rm -f statname.txt
gawk 'NF == 23 { print $16, $17, $22 }' $infile | sort | uniq > statname.txt
while read mymajor myminor mystatname; do
    read mymajor0 myminor0 <<< $(ls -l /dev | gawk '$NF == "'$mystatname'" { print $(NF-5), $(NF-4) }' | tr ',' ' ')

    if [ "x$mymajor0" == "x" ]; then
        mymajor0="0"
    fi
    if [ "x$myminor0" == "x" ]; then
        myminor0="0"
    fi

    if [ $mymajor != $mymajor0 -o $myminor != $myminor0 ]; then
        echo "  ERROR:" for $mystatname expect device major minor $mymajor $myminor but got $mymajor0 $myminor0
        retval=1
    fi
done < statname.txt

#
# For each major number, check name.
#

# Extract the major numbers and names.
gawk 'NF == 23 { print $16, $21 }' $infile | sort | uniq > majnam.txt

# Compose a D script to write a name for each major number.
gawk '{
        printf("BEGIN { x = %s }\n", $1);
        printf("BEGIN { printf(\"%%d %%s\\n\", %s, stringof(((struct blk_major_name **)&`major_names)[%s % 255]->name)); }\n",
               $1, $1);
      }' majnam.txt >> D.d
echo "BEGIN { exit(0); }" >> D.d
echo "ERROR { printf(\"%d nfs\\n\", x) }" >> D.d

$dtrace $dt_flags -qs D.d | sort | gawk 'NF != 0' > majnam.chk

if ! diff majnam.txt majnam.chk > /dev/null; then
    echo "  ERROR: major number mismatch with name"
    echo "    majnam.txt"
    cat       majnam.txt
    echo "    majnam.chk"
    cat       majnam.chk
    retval=1
fi

rm -f D.d majnam.txt majnam.chk

#
# For name:      Expect pathname:
#
# == "nfs"       "<nfs>"
# != "nfs"       "<unknown>"     # FIXME: "<unknown>"?  Really?
#

gawk '
BEGIN { err = 0 }
NF == 23 {
    if ($21 == "nfs") expect = "<nfs>";
    else              expect = "<unknown>";
    if ($23 != expect) {
        print "  ERROR: for name " $21 " expect " expect " but got " $23;
        err = 1;
    }
}
END { exit(err) }
' $infile
if [ $? -ne 0 ]; then
    retval=1
fi

#
# Check that for each name, there is a distinct major number.
# This does not guarantee that the mapping is correct, but it
# is a partial correctness check and we already checked the
# statname mapping to edev numbers against "ls -l /dev".
#

gawk 'NF == 23 { print $21, $16 }' $infile | sort | uniq > map-name-to-major.txt
nmaps=`cat map-name-to-major.txt | wc -l`
nnames=`gawk '{print $1}' map-name-to-major.txt | sort | uniq | wc -l`
nmajor=`gawk '{print $2}' map-name-to-major.txt | sort | uniq | wc -l`
if [ $nnames -ne $nmaps -o $nmajor -ne $nmaps ]; then
    echo "  ERROR: name-to-major-number is not a one-to-one mapping"
    cat map-name-to-major.txt
    retval=1
fi

#
# If the name is "nfs", the edev should be 0.  FIXME: is this correct?
#

gawk '
BEGIN { err = 0 }
$21 == "nfs" && $15 != 0 { print "  ERROR: name is nfs but edev is nonzero"; err = 1 }
END { exit(err) }' $infile
if [ $? -ne 0 ]; then
    retval=1
fi

#
# Exit.
#

exit $retval
