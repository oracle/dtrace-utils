/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -s

/*
 *  NAME
 *    io-cast-net.d - get an overview of read and write calls
 *
 *  SYNOPSIS
 *    sudo ./io-cast-net.d -c "<name-of-app> [app options]"
 *
 *  DESCRIPTION
 *    This script lists all the functions in libc that include
 *    one of the following keywords in their name:
 *      read, write, open, close
 *    This is a fairly wide net that is cast to find out which
 *    libc functions are used to open, or close a file, and which
 *    functions from libc are used to read from, or write to a
 *    file.
 *    The number of calls to each of these functions is counted,
 *    plus the total of all such calls.
 *
 *  NOTES
 *    - This is an example how you can find out more about a certain
 *    area of interest.  Wildcards and empty fields in the probe
 *    definition are ideally suited for this, but be aware not to
 *    overask and create a huge number of probes.
 *
 *    - This script could be the first step to analyze the I/O
 *    behaviour of an application, or command.
 *
 *    First, this script, io-cast-net.d, may be used on an application
 *    to identify all functions with read, write, open, and close
 *    in the name.  This list may then be examined to manually select
 *    the particular functions to trace in this application.
 *    An example of this approach is in script io-stats.d.
 *
 *    - Wildcards are used and as a result, more functions may be listed
 *    than you might be interested in, but in this way you will see all
 *    of them and can make a selection which one(s) to focus on.
 *
 *    - There are no print statements in this example, because we would
 *    like to show the default output from DTrace.  In particular, the
 *    feature that, when the tracing has finished, all aggregations are
 *    printed by default.  Since this example only uses aggregations,
 *    it means that all results are printed upon completion.
 */

/*
 *  These are the probes used to query the system and see those function
 *  calls.  The first aggregation has a key that consists of 2 fields:
 *  the name of the function and the module name.  Thanks to this key,
 *  the results are differentiated by the function name and the name
 *  of the module.
 *
 *  The latter is always going to be libc.so, e.g. libc.so.6.  We
 *  have included it in the key for the aggregation to show how
 *  to make a script more flexible.  In case the module name is
 *  left out in the probe definition, or changed, the script will
 *  still work and print the module name(s) of the probe(s) that
 *  fired.
 *
 *  The second aggregation has no key.  That means that the count is
 *  incremented each time one of the probes fires.  That will give us
 *  the total count across all the probes that fired.
 *
 *  The output shows a table with 3 columns: the name of the function,
 *  the module (which should be libc), and the call count.  The results
 *  are sorted by the count.  This table is followed by a single number.
 *  It is the total of number of calls that have been traced.
 */

pid$target:libc.so:*read*:entry,
pid$target:libc.so:*write*:entry,
pid$target:libc.so:*open*:entry,
pid$target:libc.so:*close*:entry
{
  @target_calls[probefunc,probemod] = count();
  @total_count                      = count();
}
