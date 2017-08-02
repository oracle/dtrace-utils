/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *	Checks that buffer space for an END enabling is always reserved in a
 *	fill buffer.  This will fail because the size of the END enabling
 *	(64 bytes) exceeds the size of the buffer (32 bytes).
 *
 * SECTION: Buffers and Buffering/fill Policy;
 *	Buffers and Buffering/Buffer Sizes;
 *	Options and Tunables/bufpolicy;
 *	Options and Tunables/bufsize;
 *	Options and Tunables/strsize
 */

#pragma D option bufpolicy=fill
#pragma D option bufsize=32
#pragma D option strsize=64

BEGIN
{
	exit(0);
}

END
{
	trace(execname);
}
