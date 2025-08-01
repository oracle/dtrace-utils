#!/usr/sbin/dtrace -s

/*
 *  NAME
 *    thread-ids.d - show the mapping between Pthread IDs and tid values
 *
 *  SYNOPSIS
 *    sudo ./thread-ids.d -c "<name-of-app> [app options]"
 *
 *  DESCRIPTION
 *    This script assumes that the target uses the Pthreads library
 *    to create one or more threads.  It shows the mapping of the
 *    Pthread thread IDs and the thread ID, as returned in the tid
 *    built-in variable.
 *
 *  NOTES
 *    - In addition to showing how to uncover this mapping, this
 *    script also shows a technique how to retrieve a value from
 *    a pointer argument in a function call.
 *
 *    In this case, this is the thread ID that pthread_create()
 *    returns in its first argument.
 *
 *    This is from the man page for pthread_create():
 *
 *    int pthread_create(pthread_t *restrict thread,
 *                        const pthread_attr_t *restrict attr,
 *                        void *(*start_routine)(void *),
 *                        void *restrict arg);
 *
 *    We need to capture the contents of *thread.
 *
 *    In this case, we cannot use the built-in tid variable
 *    within pthread_create(), because typically, this function
 *    is executed by one thread, the main thread.  This means
 *    that the value in tid is the thread ID of this main thread,
 *    but we need to have the value of the thread that is created
 *    as a result of calling pthread_create().  As shown below,
 *    this can be done by tracing clone3(), which is called by
 *    pthread_create().

 *    - It is assumed that a function called main is executed.
 *    If this is not the case, this is not a critical error.
 *    The first probe is used to capture the tid value for the main
 *    program.  This thread is however not created by function
 *    pthread_create() and therefore this part of the script is
 *    not essential.
 *    This is why this probe and corresponding printf() statement
 *    in the END probe can safely be removed, or replaced by a
 *    suitable alternative.
 */

/*
 *  Suppress the default output from the dtrace command and have
 *  printa() print the aggregation data sorted by the first field.
 */
#pragma D option quiet
#pragma D option aggsortkey=1
#pragma D option aggsortkeypos=0

/*
 *  Declare a thread-local variable.  This is to ensure that the
 *  compiler sees it before it is referenced.
 */
self int clone_tid;

/*
 *  Store the thread ID of the main thread.
 */
pid$target:a.out:main:entry
{
  tid_main = tid;
}

/*
 *  Variable pthr_id_p captures the first argument of the call to
 *  pthread_create().  This is a pointer, but we can't dereference
 *  it here, because the contents this pointer points to, are only
 *  available upon return.
 *  This is solved by storing the pointer here.  In the return probe
 *  for this function, we can then dereference the pointer.
 */
pid$target:libc.so:pthread_create:entry
{
  self->pthr_id_p = (int64_t *) arg0;
}

/*
 *  We actually know that clone3() is called.  By using the wildcard
 *  here, the script continues to work in case this number changes in
 *  the future.
 */
pid$target:libc.so:clone*:return
/ self->pthr_id_p !=NULL /
{
/*
 *  We know that one of the clone functions is called from within
 *  pthread_create() and it returns the thread ID that DTrace stores
 *  in the tid variable.
 *  This is why the return value, which is stored in arg1, is copied
 *  into a thread-local variable called clone_tid.  This variable is
 *  then referenced in the return probe for pthread_create().
 */
  self->clone_tid = arg1;
}

/*
 *  This is where things come together.
 *
 *  We already have the value for the thread ID as used by DTrace.
 *  It is stored in thread-local variable clone_tid.
 *
 *  Now we can capture the Pthreads thread ID.
 *
 *  There is one more thing to this though.  Below we use an
 *  aggregation to store the results (and ignore the count when
 *  printing the results), but this is not strictly necessary.
 *
 *  The approach chosen here allows us to control the sorting of
 *  the results.  In this case the sort field has been set to the
 *  Pthreads thread ID, but this can easily be changed.
 *
 *  If there is no need to print the data sorted, a simple printf()
 *  will do.  All that needs to be done then is to print the two
 *  variables this->pthr_id and self->clone_tid.
 *
 */
pid$target:libc.so:pthread_create:return
{
/*
 *  We are about to return from pthread_create() and can dereference
 *  the pointer.
 *  Before we do so, the data needs to be copied from user space into
 *  the kernel.  Since this is a 64 bit address, 8 bytes are copied.
 *  The value is what pthread_create() returns in its *thread first
 *  argument, which is the Pthreads thread ID.
 *  This gives us both thread IDs and they are used in the key for the
 *  aggregation called thread_mapping.
 */
  this->pthr_id = *(int64_t *) copyin(*self->pthr_id_p,8);
  @thread_mapping[this->pthr_id,self->clone_tid] = count();

/*
 *  Free the storage for the thread-local variables.
 */
  self->pthr_id_p      = 0;
  self->clone_tid      = 0;
}

/*
 *  The aggregation is printed in the END probe.  We use printf()
 *  statements to print the thread ID of the main program and the
 *  table header.
 *  Note that there is no format field for the value for the
 *  aggregation.  As explained above, the value is not relevant
 *  in this case.
 *
 *  Note that we do not need to include these print statements,
 *  because aggregations that are not explictly printed, are
 *  automatically printed when the script terminates.  The reason
 *  we print them ourselves is to have control over the lay-out.
 */
END
{
  printf("Thread ID of main program: %d\n\n",tid_main);
  printf("%16s <=> %-9s\n\n","Pthreads ID","Thread ID");
  printa("%16d <=> %-9d\n",@thread_mapping);
}
