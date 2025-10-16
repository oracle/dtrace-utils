#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
#------------------------------------------------------------------------------
# This example embeds a DTrace script in a bash script.  The bash script
# is used to set the variables needed in the D script.
#
# fsact -- Display cumulative read and write activity across a file
#          system device
#
#          Usage: fsact [<filesystem>]
#------------------------------------------------------------------------------

#------------------------------------------------------------------------------
# If no file system is specified, assume /
#------------------------------------------------------------------------------
[ $# -eq 1 ] && FSNAME=$1 || FSNAME="/"
[ ! -e $FSNAME ] && echo "$FSNAME not found" && exit 1

#------------------------------------------------------------------------------
# Determine the mountpoint, major and minor numbers, and file system size.
#------------------------------------------------------------------------------
MNTPNT=$(df $FSNAME | gawk '{ getline; print $1; exit }')
MAJOR=$(printf "%d\n" 0x$(stat -Lc "%t" $MNTPNT))
MINOR=$(printf "%d\n" 0x$(stat -Lc "%T" $MNTPNT))
FSSIZE=$(stat -fc "%b" $FSNAME)

#------------------------------------------------------------------------------
# Run the embedded D script.
#------------------------------------------------------------------------------
sudo dtrace -qs /dev/stdin << EOF
/*
 *  DESCRIPTION
 *    The io:::done probe from the io provider is used to get read and write
 *    statistics.  In particular, the id of the block that is accessed for
 *    the read or write operation.
 */

BEGIN
{
  printf("Show how often blocks are accessed in read and write operations\n");
  printf("The statistics are updated every 5 seconds\n");
  printf("The block IDs are normalized to a scale from 0 to 10\n");
  printf("The file system is %s\n","$FSNAME");
  printf("The mount point is %s\n","$MNTPNT");
  printf("The file system size is %s bytes\n","$FSSIZE");
}

/*
 *  This probe fires after an I/O request has been fulfilled. The
 *  done probe fires after the I/O completes, but before completion
 *  processing has been performed on the buffer.
 *
 *  A pointer to structure devinfo_t is in args[1].  This is used
 *  to get the major and minor number of the device.
 *
 *  A pointer to structure bufinfo_t is in args[0].  This is used
 *  to get the flags and the block number.
 */
io:::done
/ args[1]->dev_major == $MAJOR && args[1]->dev_minor == $MINOR /
{
/*
 *  Check if B_READ has been set and assign a string to io_type
 *  based on the outcome of the check.  This string is used as
 *  the key in aggregation io_stats.
 */
  io_type = args[0]->b_flags & B_READ ? "READ" : "WRITE";

/*
 *  Structure member b_lblkno identifies which logical block on the
 *  device is to be accessed.  Normalize thise block number as an
 *  integer in the range 0 to 10.
 */
  blkno = (args[0]->b_blkno)*10/$FSSIZE;

/*
 *  Aggregate blkno linearly over the range 0 to 10 in steps of 1.
 */
  @io_stats[io_type] = lquantize(blkno,0,10,1)
}

/*
 *  Fires every 5 seconds.
 */
profile:::tick-5s
{
  printf("%Y\n",walltimestamp);

/*
 *  Display the contents of the aggregation.
 */
  printa("%s\n%@d\n",@io_stats);

/*
 *  Reset the aggregation every time this probe fires
 */
  clear(@io_stats);
}

/*
 *  Fires every 21 seconds.  Since exit() is called, the tracing terminates
 *  the first time this probe fires and the clause is executed.
 */
profile:::tick-21s
{
  printf("Tracing is terminated now\n");
  exit(0);
}
EOF
