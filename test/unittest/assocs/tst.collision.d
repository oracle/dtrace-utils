/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Dynamic variables do not overwrite eachother.
 */

BEGIN
{
	assoc[0x1234] = 1;
	self->tls_assoc[1] = 0x1111;
	self->tls_assoc[1] = 0;
	self->tls_assoc[1] = 0x2222;

	printf("%x / %x\n", assoc[0x1234], self->tls_assoc[1]);

	exit(assoc[0x1234] == 1 && self->tls_assoc[1] == 0x2222 ? 0 : 1);
}
