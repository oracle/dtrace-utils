/*
 * Oracle Linux DTrace.
 * Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 - uses reading from kernel addresses */

/*
 * ASSERTION:
 *	A member named after a D keyword can be referenced, both
 *      after -> and after ., even with comments interjected.
 */

BEGIN
{
	printf("%p\n", &`cgrp_dfl_root.cgrp.self);
	printf("%p\n", &`cgrp_dfl_root.cgrp.self.cgroup->self);
	printf("%p\n", `cgrp_dfl_root.cgrp.self.cgroup->self.cgroup);
	printf("%p\n", &`cgrp_dfl_root.cgrp./* foo */self.cgroup->self);
	printf("%p\n", `cgrp_dfl_root.cgrp.self.cgroup->/* foo */self.cgroup);
	printf("%p\n", &`cgrp_dfl_root.cgrp.self/* foo */.cgroup->self);
	printf("%p\n", `cgrp_dfl_root.cgrp.self.cgroup->self/* foo */.cgroup);

	exit(0);
}

