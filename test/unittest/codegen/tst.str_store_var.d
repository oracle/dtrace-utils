/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option rawbytes
#pragma D option strsize=6
#pragma D option quiet

BEGIN
{
	/* Global */
	g = "abcdefgh"; trace(g); trace(g + 1); trace(1 + g);
	g = "12"      ; trace(g); trace(g + 1); trace(1 + g);

	/* (Clause) local */
	this->l = "abcdefgh"; trace(this->l); trace(this->l + 1); trace(1 + this->l);
	this->l = "12"      ; trace(this->l); trace(this->l + 1); trace(1 + this->l);

	/* Thread local */
	self->t = "abcdefgh"; trace(self->t); trace(self->t + 1); trace(1 + self->t);
	self->t = "12"      ; trace(self->t); trace(self->t + 1); trace(1 + self->t);

	/* Associative */
	a[1234] = "abcdefgh"; trace(a[1234]); trace(a[1234] + 1); trace(1 + a[1234]);
	a[1234] = "12"      ; trace(a[1234]); trace(a[1234] + 1); trace(1 + a[1234]);

	exit(0);
}

ERROR
{
	exit(1);
}
