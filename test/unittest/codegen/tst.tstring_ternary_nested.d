/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Ensure tstrings are handled correctly for nested ternary (?:) expressions.
 *
 * The trace() action below is annotated with the tstring slots that are
 * allocated, or transfered from nested expressions.
 *
 * - Each strjoin() of two string constants allocates a tstring.
 * - Each strjoin() of two tstrings allocates a tstring (freeing the two args).
 * - Each ternary re-uses the tstring of its left child (?-expression) (freeing
 *   the right child tstring (:-expression).
 *
 * The nested expression below consumes 6 tstrings, which is the current limit.
 */

#pragma D option quiet

BEGIN {
	x = 2;
	trace(x > 3						/* slot 2 */
		? strjoin(					/*   slot 2 */
			strjoin("a", "bc"),			/*     slot 0 */
			strjoin("de", "f"))			/*     slot 1 */
		: x > 2						/* slot 3 */
			? strjoin(				/*   slot 3 */
				strjoin("A", "BC"),		/*     slot 0 */
				strjoin("DE", "F"))		/*     slot 1 */
			: x > 1					/* slot 4 */
				? strjoin(			/*   slot 4 */
					strjoin("u", "vw"),	/*     slot 0 */
					strjoin("xy", "z"))	/*     slot 1 */
				: strjoin(			/*   slot 5 */
					strjoin("U", "VW"),	/*     slot 0 */
					strjoin("XY", "Z")));	/*     slot 1 */

	exit(0);
}

ERROR {
	exit(1);
}
