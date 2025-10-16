/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    rwdiskact.d - for block devices show the read() and write() performance
 *
 *  SYNOPSIS
 *    sudo dtrace -s rwdiskact.d
 *
 *  DESCRIPTION
 *    The io provider is used to display the throughput of the read()
 *    and write calls() for the block devices on the system.  The
 *    tracing automatically stops after 10 seconds.
 *
 *  NOTES
 *   - The bufinfo_t structure is the abstraction that describes an I/O
 *   request.  The buffer that corresponds to an I/O request is pointed
 *   to by args[0] in the start, done, wait-start, and wait-done probes
 *   available through the io provider.
 *
 *   - Detailed information about this data structure can be found in
 *   the DTrace User Guide.  For more details, you can also check
 *   /usr/lib64/dtrace/<version>/io.d, where <version> denotes the
 *   kernel version.
 *
 *    - Although the results of an aggregation are automatically
 *    printed when the tracing terminates, in this case, we want to
 *    control the format of the output.  This is why the results are
 *    printed using printa() in the END probe
 */

/*
 *  To avoid that the carefully crafted output is mixed with the
 *  default output by the dtrace command, enable quiet mode.
 */
#pragma D option quiet

/*
 *  Fires every 10 seconds.  Since exit() is called, the tracing terminates
 *  the first time this probe fires and the clause is executed.
 */
profile:::tick-10sec
{
  exit(0);
}

/*
 *  The pointer to bufinfo_t is in args[0].  Here it is used to get
 *  b_flags (the flags), b_edev (the extended device) and b_blkno (the
 *  expanded block number on the device).  These three fields are used
 *  in the key for associative array io_start.
 */
io:::start
{
  io_type = args[0]->b_flags & B_READ ? "READ" : "WRITE";
  io_start[args[0]->b_edev, args[0]->b_blkno, io_type] = timestamp;
}

io:::done
{
/*
 *  We would like to show the throughput to a device in KB/sec, but
 *  the values that are measured are in bytes and nanoseconds.
 *  You want to calculate the following:
 *
 *  bytes / 1024
 *  ------------------------
 *  nanoseconds / 1000000000
 *
 *  As DTrace uses integer arithmetic and the denominator is usually
 *  between 0 and 1 for most I/O, the calculation as shown will lose
 *  precision.  So, restate the fraction as:
 *
 *  bytes         1000000000      bytes * 976562
 *  ----------- * ------------- = --------------
 *  nanoseconds   1024            nanoseconds
 *
 *  This is easy to calculate using integer arithmetic.
 */
  io_type = args[0]->b_flags & B_READ ? "READ" : "WRITE";
  this->elapsed = timestamp -
                  io_start[args[0]->b_edev,args[0]->b_blkno,io_type];

/*
 *  The pointer to structure devinfo_t is in args[1].  Use this to get the
 *  name (+ instance/minor) and the pathname of the device.
 *
 *  Use the formula above to compute the throughput.  The number of bytes
 *  transferred is in bufinfo_t->b_bcount
 */
  @io_throughput[strjoin("device name = ",args[1]->dev_statname),
                 strjoin("path = ",args[1]->dev_pathname),
                io_type] =
                   quantize((args[0]->b_bcount * 976562) / this->elapsed);

/*
 *  Free the storage for the entry in the associative array.
 */
  io_start[args[0]->b_edev, args[0]->b_blkno,io_type] = 0;}

/*
 *  Use a format string to print the aggregation.
 */
END
{
  printa(" %s (%s) %s \n%@d\n", @io_throughput);
}
