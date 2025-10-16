/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 *  NAME
 *    errno1.d - list and count the system calls with a non-zero errno value
 *
 *  SYNOPSIS
 *    sudo dtrace -s errno1.d
 *
 *  DESCRIPTION
 *    Trace every system call that returns a non-zero value in errno.
 *    Show the process name, the name of the function it executes, the
 *    user id, the name of the error that corresponds to the value of
 *    errno, a descriptive message for the error, and how often this
 *    combination occurred.
 *
 *  NOTES
 *    - The script needs to be terminated with ctrl-C.  In case the
 *    script is running in the background, get it to run in the
 *    foreground first by using the fg command and then use ctrl-C
 *    to terminate the process.  Otherwise, typing in ctrl-C will do.
 *
 *    - The value of errno is available upon the return from the system call.
 *
 *    - To present the results in a compact form, we use an aggregation
 *    called syscalls.  Otherwise we may get several screens with the
 *    information.  Plus that we then can't easily count the functions.
 *
 *    - Although the results of an aggregation are automatically
 *    printed when the tracing terminates, in this case, we want to
 *    control the format of the output.  This is why the results are
 *    printed using printa() in the END probe
 */

BEGIN
{
/*
 *  Define an associative array called errno_code that maps a value
 *  of errno to an enum name.  This information can be found in file
 *  /usr/include/asm-generic/errno-base.h.
 *
 *  File /usr/include/asm-generic/errno.h has the codes for calling a system
 *  call that does not exist.  This has not been used here though.
 */
  errno_code[EPERM]   = "EPERM";    /* Operation not permitted */
  errno_code[ENOENT]  = "ENOENT";   /* No such file or directory */
  errno_code[ESRCH]   = "ESRCH";    /* No such process */
  errno_code[EINTR]   = "EINTR";    /* Interrupted system call */
  errno_code[EIO]     = "EIO";      /* I/O error */
  errno_code[ENXIO]   = "ENXIO";    /* No such device or address */
  errno_code[E2BIG]   = "E2BIG";    /* Argument list too long */
  errno_code[ENOEXEC] = "ENOEXEC";  /* Exec format error */
  errno_code[EBADF]   = "EBADF";    /* Bad file number */
  errno_code[ECHILD]  = "ECHILD";   /* No child processes */
  errno_code[EAGAIN]  = "EAGAIN";   /* Try again or operation would block */
  errno_code[ENOMEM]  = "ENOMEM";   /* Out of memory */
  errno_code[EACCES]  = "EACCES";   /* Permission denied */
  errno_code[EFAULT]  = "EFAULT";   /* Bad address */
  errno_code[ENOTBLK] = "ENOTBLK";  /* Block device required */
  errno_code[EBUSY]   = "EBUSY";    /* Device or resource busy */
  errno_code[EEXIST]  = "EEXIST";   /* File exists */
  errno_code[EXDEV]   = "EXDEV";    /* Cross-device link */
  errno_code[ENODEV]  = "ENODEV";   /* No such device */
  errno_code[ENOTDIR] = "ENOTDIR";  /* Not a directory */
  errno_code[EISDIR]  = "EISDIR";   /* Is a directory */
  errno_code[EINVAL]  = "EINVAL";   /* Invalid argument */
  errno_code[ENFILE]  = "ENFILE";   /* File table overflow */
  errno_code[EMFILE]  = "EMFILE";   /* Too many open files */
  errno_code[ENOTTY]  = "ENOTTY";   /* Not a typewriter */
  errno_code[ETXTBSY] = "ETXTBSY";  /* Text file busy */
  errno_code[EFBIG]   = "EFBIG";    /* File too large */
  errno_code[ENOSPC]  = "ENOSPC";   /* No space left on device */
  errno_code[ESPIPE]  = "ESPIPE";   /* Illegal seek */
  errno_code[EROFS]   = "EROFS";    /* Read-only file system */
  errno_code[EMLINK]  = "EMLINK";   /* Too many links */
  errno_code[EPIPE]   = "EPIPE";    /* Broken pipe */
  errno_code[EDOM]    = "EDOM";     /* Math argument out of domain of func */
  errno_code[ERANGE]  = "ERANGE";   /* Math result not representable */

/*
 *  This associative array called errno_msg has a brief description of the
 *  error for each non-zero value of errno.
 */
  errno_msg[EPERM]   = "Operation not permitted";
  errno_msg[ENOENT]  = "No such file or directory";
  errno_msg[ESRCH]   = "No such process";
  errno_msg[EINTR]   = "Interrupted system call";
  errno_msg[EIO]     = "I/O error";
  errno_msg[ENXIO]   = "No such device or address";
  errno_msg[E2BIG]   = "Argument list too long";
  errno_msg[ENOEXEC] = "Exec format error";
  errno_msg[EBADF]   = "Bad file number";
  errno_msg[ECHILD]  = "No child processes";
  errno_msg[EAGAIN]  = "Try again or operation would block";
  errno_msg[ENOMEM]  = "Out of memory";
  errno_msg[EACCES]  = "Permission denied";
  errno_msg[EFAULT]  = "Bad address";
  errno_msg[ENOTBLK] = "Block device required";
  errno_msg[EBUSY]   = "Device or resource busy";
  errno_msg[EEXIST]  = "File exists";
  errno_msg[EXDEV]   = "Cross-device link";
  errno_msg[ENODEV]  = "No such device";
  errno_msg[ENOTDIR] = "Not a directory";
  errno_msg[EISDIR]  = "Is a directory";
  errno_msg[EINVAL]  = "Invalid argument";
  errno_msg[ENFILE]  = "File table overflow";
  errno_msg[EMFILE]  = "Too many open files";
  errno_msg[ENOTTY]  = "Not a typewriter";
  errno_msg[ETXTBSY] = "Text file busy";
  errno_msg[EFBIG]   = "File too large";
  errno_msg[ENOSPC]  = "No space left on device";
  errno_msg[ESPIPE]  = "Illegal seek";
  errno_msg[EROFS]   = "Read-only file system";
  errno_msg[EMLINK]  = "Too many links";
  errno_msg[EPIPE]   = "Broken pipe";
  errno_msg[EDOM]    = "Math argument out of domain of func";
  errno_msg[ERANGE]  = "Math result not representable";
}

/*
 *  Store the information in an aggregation called syscalls.
 *  Use the predicate to only allow non-zero errno values that are
 *  within the range for errno.
 */
syscall:::return
/ errno > 0 && errno <= ERANGE /
{
  @syscalls[execname,probefunc,uid,errno_code[errno],
            errno_msg[errno]] = count();
}

/*
 *  The printf() line prints the header of the table to follow.
 */
END
{
  printf("%-20s %-16s %-6s %-7s %-35s %5s\n\n","PROCESS","SYSCALL","UID",
         "ERROR","DESCRIPTION","COUNT");
  printa("%-20s %-16s %-6d %-7s %-35s %@5d\n",@syscalls);
}
