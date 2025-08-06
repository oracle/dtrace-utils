/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'chip' inline variable returns the correct value.
 */

#pragma D option quiet

BEGIN
{
	trace(chip);
}

BEGIN
/chip == curcpu->cpu_chip/
{
	exit(0);
}

BEGIN,
ERROR {
	exit(1);
}
