/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Storing 0 into a TLS variable removes it from storage, making
 *	      room for another variable, even if it is a struct.
 *
 * SECTION: Variables/Thread-Local Variables
 */

#pragma D option quiet
#pragma D option dynvarsize=15

this struct {
	int a;
} val;

BEGIN
{
	this->val.a = 1;
	self->a = this->val;
	this->val.a = 2;
	self->b = this->val;
	this->val.a = 3;
	self->c = this->val;
	self->a = 0;
	this->val.a = 4;
	self->d = this->val;
	trace(self->b.a);
	trace(self->c.a);
	trace(self->d.a);
	exit(0);
}

ERROR
{
	exit(1);
}
