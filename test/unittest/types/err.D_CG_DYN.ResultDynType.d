/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * An expression cannot evaluate to result of dynamic type.
 *
 * SECTION: Errtag/D_CG_DYN
 */

#pragma D option quiet

translator lwpsinfo_t < int i >
{
	pr_flag = i;
};

BEGIN
{
	xlate < lwpsinfo_t * > (0);
	exit(0);
}
