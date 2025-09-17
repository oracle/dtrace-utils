/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  NAME
 *    io-stats.d - show several I/O related statistics
 *
 *  SYNOPSIS
 *    sudo ./io-stats.d -c "<name-of-app> [app options]"
 *
 *  DESCRIPTION
 *    This script shows several I/O statistics for the target
 *    application, or command.  Among others, the filename, the
 *    file descriptor, or stream it is connected to, the number
 *    of bytes read, written, or both, are printed.
 *
 *  NOTES
 *    - All the probes but the END probe in this script are based
 *      upon the pid provider.
 *
 *    - This script is quite elaborate and has several probes.
 *    More specifically, in alphabetical order, these are the
 *    functions from the libc library that are traced:
 *     close()
 *     fclose()
 *     fopen()
 *     fread()
 *     fwrite()
 *     open()
 *     pwrite()
 *     read()
 *    In the probes, we rely on the information from the man pages
 *    for these functions to identify arguments of interest and
 *    the return values.
 *
 *    - Not all of the probes may be relevant to your case.  While
 *    no harm is done if a probe does not fire, the associated
 *    aggregation(s) will be empty, but still printed.  This is because
 *    we use printa() to print the aggregation(s).  If printa() is not
 *    used for an aggregation, nothing is printed if it is empty.  You
 *    can of course always remove such irrelevant probes and related
 *    printa() and printf()  statements.
 *
 *    - A related script is io-cast-net.d.  You may want to run this
 *    first to cast a fairly wide net and explore which open, close,
 *    read and write functions are called when executing the target.
 *
 *    - All print statements are in the END probe.  On purpose
 *    several different format strings are used.  This is done to
 *    demonstrate the flexibility in presenting the results.
 *
 *    Note that we do not need to include these print statements,
 *    because aggregations that are not explictly printed, are
 *    automatically printed when the script terminates.  The reason
 *    we print them ourselves is to have control over the lay-out.
 *    Another thing we do is to print more than one aggregation
 *    with a single printa() statement.
 */

/*
 *  Suppress the default output from the dtrace command and have
 *  printa() print the aggregation data sorted by the first field.
 */
#pragma D option quiet
#pragma D option aggsortkey=1
#pragma D option aggsortkeypos=0

/*
 *  Capture the name of the file that needs to be opened, in the call
 *  to any function that starts with fopen.  The first argument to
 *  this function contains the name of the file to be opened.
 *  As arg0 is of type integer, it is converted to a string, copied
 *  from user space and stored in the thread-local variable fname_fopen.
 */
pid$target:libc.so:fopen*:entry
{
  self->fname_fopen = copyinstr(arg0);
}

/*
 *  This probe is nearly identical to the one above, but note that
 *  for this probe, the name of the thread-local variable is slightly
 *  different and called fname_open.
 */
pid$target:libc.so:open:entry
{
  self->fname_open = copyinstr(arg0);
}

/*
 *  This is the return probe for any of the fopen calls.  The predicate
 *  checks that fname_fopen has been set and if so, in the clause the
 *  count is updated and the new value is stored in an aggregation
 *  with 3 fields: the name of the function, the name of the
 *  file and the return value of the function, which is stored
 *  in arg1.
 *  Note that we can indeed access this file name.  Although set in the
 *  corresponding entry probe, a thread-local variable is accessible in
 *  the data space of the corresponding thread and so can be read, or
 *  written, here.
 *  The third field in the aggregation is arg1.  This is the return value
 *  of the function and is the pointer to the stream.
 *  Since variable fname_fopen is no longer needed, the storage is freed.
 */
pid$target:libc.so:fopen*:return
/ self->fname_fopen != 0/
{
  @file_pointer[probefunc,self->fname_fopen,arg1] = count();
  self->fname_fopen = 0;
}

/*
 *  This probe is nearly identical to the previous probe.  Other than
 *  the name of the thread-local variable, the difference is in the
 *  name of the aggregation.  This is because the return value, the
 *  file descriptor, is of type integer.  This means that we need
 *  a different format string when printing this field.
 */
pid$target:libc.so:open:return
/ self->fname_open != 0/
{
  @file_descriptor[probefunc,self->fname_open,arg1] = count();
  self->fname_open = 0;
}

/*
 *  This probe stores the name of the function, which is fclose(),
 *  and the argument, the pointer to the stream.
 */
pid$target:libc.so:fclose:entry
{
  @fclose_stream[probefunc,arg0] = count();
}

/*
 *  This probe is very similar to the one above.  Again, the difference
 *  is in the name of the aggregation, and the argument which is the
 *  file descriptor.  It is of type integer.  We need to know this when
 *  printing the key.
 */
pid$target:libc.so:close:entry
{
  @close_fd[probefunc,arg0] = count();
}

/*
 *  One of the functions traced, is pwrite().  The first argument, arg0,
 *  is the file descriptor.  The third argument is arg2.  This contains
 *  the number of bytes requested to be written.
 *  The aggregation function sum() is used to sum up all those byte counts.
 *  As a result, the aggregation contains the total number of bytes
 *  requested to be written to the file connected to the file descriptor.
 */
pid$target:libc.so:pwrite:entry
{
  @pwrite_bytes[probefunc,arg0] = sum(arg2);
}

/*
 *  The pwrite() function returns the number of bytes actually written.
 *  This value is added to the existing contents of the aggregation.  The
 *  name of the function, pwrite, is the only field in the key, so this
 *  aggregation contains the total number of bytes written by pwrite().
 */
pid$target:libc.so:pwrite:return
{
  @pwrite_total_bytes[probefunc] = sum(arg1);
}

/*
 *  This probe accumulates the number of bytes the read() function requests
 *  to be read.  The aggregation is differentiated by the file descriptor.
 *  This means that we get the totals on a per file descriptor basis.
 *  We also store the file descriptor in a thread-local variable called fd.
 *  A thread-lcoal variable is used because we would like to reference it in
 *  the return probe for this function.
 */
pid$target:libc.so:read:entry
{
  self->fd = arg0;
  @bytes_requested[probefunc,self->fd] = sum(arg2);
}

/*
 *  The read() function returns the actual number of bytes read.  These
 *  values are accumulated in aggregation bytes_actually_read that includes
 *  the file descriptor in the key.
 *  We also accumulate the total number of bytes actually read across all
 *  the file descriptors and store this in aggregation total_bytes_read.
 *  Note that this aggregation has no key.
 *  Since thread-local variable fd is no longer needed, the storage is
 *  freed.
 */
pid$target:libc.so:read:return
{
  @bytes_actually_read[probefunc,self->fd] = sum(arg1);
  @total_bytes_read                        = sum(arg1);
  self->fd = 0;
}

/*
 *  This is a shared clause for the probes for the fread() and fwrite()
 *  functions.  We can share the clause, because the arguments and return
 *  value are the same for both functions.
 */
pid$target:libc.so:fread:entry,
pid$target:libc.so:fwrite:entry
{
/*
 *  This probe traces the entry to functions fread() and fwrite().
 *  A clause-local variable called comment is used to store a dynamically
 *  generated string.  This string is then concatenated with the name of the
 *  function, resulting in a string, stored in clause-local variable rw,
 *  that depends on the function name in the probe.
 */
  this->comment = (probefunc == "fread") ?
                   "read by " : "written by ";
  this->rw = strjoin(this->comment,probefunc);

/*
 *  The aggregation has a key that consists of the pointer to the stream and
 *  the generated string.  This aggregation accumulates the product of the
 *  size in bytes and the number of elements, which is the total number of
 *  bytes read, or written.
 */
  @total_byte_count_frw[arg3,this->rw] = sum(arg1*arg2);
}

/*
 *  This probe accumulates the return value of the fread() function, which
 *  is the number of elements read.  This number is accumulated across all
 *  pointers to streams.
 */
pid$target:libc.so:fread:return
{
  @fread_total_elements[probefunc] = sum(arg1);
}

/*
 *  The output section where all the results are printed.  In one case,
 *  we use 2 aggregations in the call to printa().  This is supported if
 *  the key is the same.
 *
 *  Note that we do not need to include these print statements, because
 *  aggregations that are not explictly printed, are automatically printed
 *  when the script terminates.  The reason we print them ourselves is
 *  to have control over the lay-out.
 *  Another thing we do is to print more than one aggregation with a
 *  single printa() statement.
 */
END
{
  printf("%8s %20s   %-15s %6s\n",
               "Function","Filename","File pointer","Count");
  printa("%8s %20s   0x%-13p %@6d\n",@file_pointer);

  printf("\n%8s %20s   %-15s %6s\n",
               "Function","Filename","File descriptor","Count");
  printa("%8s %20s   %-15d %@6d\n",@file_descriptor);

  printf("\n%8s   %-16s %7s\n","Function","Stream/FD","Count");
  printa("%8s   0x%-14p %@7d\n",@fclose_stream);
  printa("%8s   %-16d %@7d\n",@close_fd);

  printf("\n");
  printf("%8s %5s %14s\n","Function","FD","Bytes written");
  printa("%8s %5d %@14d\n",@pwrite_bytes);
  printa("Total bytes written by %s: %@ld\n",@pwrite_total_bytes);

  printa("\nOn stream %p - Total bytes %s = %@ld\n",
                                  @total_byte_count_frw);

  printa("Total elements read by %s = %@ld\n",
                                  @fread_total_elements);

  printf("\n%33s\n","Bytes read");
  printf("%8s %6s %10s %10s\n","Function","FD","Requested","Actual");
  printa("%8s %6d %@10d %@10d\n",@bytes_requested,
                                 @bytes_actually_read);
  printa("\nTotal number of bytes read: %@d\n",@total_bytes_read);
}
