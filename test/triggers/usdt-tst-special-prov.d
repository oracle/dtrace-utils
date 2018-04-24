/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

provider test_prov {
	probe tail();
	probe enable();
	probe enable_or();
	probe enable_or_2();
	probe enable_and();
	probe enable_and_2();
	probe enable_stmt();
	probe enable_return();
	probe enable_combined();
};
