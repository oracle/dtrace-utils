/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@skip: provider declaration - not a test */

provider test_prov {
	probe place(uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3,
		    uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7,
		    uint64_t a8, uint64_t a9);
};
