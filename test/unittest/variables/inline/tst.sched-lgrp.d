/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: The 'lgrp' inline variable returns the correct value.
 */

#pragma D option quiet

BEGIN
{
	trace(lgrp);
}

BEGIN
/lgrp == curcpu->cpu_lgrp/
{
	exit(0);
}

BEGIN,
ERROR {
	exit(1);
}
