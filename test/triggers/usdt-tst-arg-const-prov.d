/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@skip: provider declaration - not a test */

provider test_prov {
	probe uval1(uint8_t a);
	probe sval1(int8_t a);
	probe uval2(uint16_t a);
	probe sval2(int16_t a);
	probe uval4(uint32_t a);
	probe sval4(int32_t a);
	probe uval8(uint64_t a);
	probe sval8(int64_t a);
};
