/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

provider test_prov {
	probe deref(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e,
		    int64_t f, int64_t g, int64_t h, int64_t i, int64_t j);
};
