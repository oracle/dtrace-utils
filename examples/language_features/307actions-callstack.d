/*
 * Linux DTrace
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#!/usr/sbin/dtrace -qs

/*
 *  SYNOPSIS
 *    sudo ./307actions-callstack.d
 *
 *  DESCRIPTION
 *    In DTrace, one can get information about the call stack.
 */

/* have a probe fire somewhere in the kernel */
fbt::ksys_write:entry
{
	/* dump the kernel call stack */
	stack();

	/* a built-in variable gives the number of frames */
	printf("%d frames\n", stackdepth);

	/* another built-in variable gives the caller PC */
	printf("second frame is the caller %p\n", caller);

	/* we can get the symbolic name of its module */
	printf(  "  module   ");  mod(caller);

	/* we can get the symbolic name of its function */
	printf("\n  function "); func(caller);

	/* sym() is an alias for func() */
	printf("\n  == symbol");  sym(caller);

	/*
	 * Similarly, we can also get the user-space call stack
	 * through ustack().  The related built-in variables are
	 * ustackdepth and ucaller.  Conversion to symbolic names
	 * can be made via umod(), ufunc(), usym(), and uaddr().
	 * User-space call stacks face a few challenges:
	 *
	 * - The BPF helper functions rely on frame-pointer
	 *   chasing, which can be impaired or even incorrect
	 *   unless -fno-omit-frame-pointer is specified.
	 *
	 * - Conversion to symbolic names does not take place when
	 *   the probe fires but later, in user space, when results
	 *   are printed.  It is possible that the user application
	 *   has already terminated and the information about its
	 *   address space gone.
	 *
	 * - For a stripped executable, you must specify
	 *   --export-dynamic when linking the program.
	 */

	exit(0);
}
