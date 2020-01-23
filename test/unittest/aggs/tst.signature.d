/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * This is a simple test to make sure that signature checking works properly
 * for the fake-o types.
 */
BEGIN
{
	@stk[ustack()] = count();
	@symmy[sym(0)] = count();
	@usymmy[usym(0)] = count();
	@funky[func(0)] = count();
	@ufunky[ufunc(0)] = count();
	@moddy[mod(0)] = count();
	@umoddy[umod(0)] = count();
}

BEGIN
{
	@stk[ustack()] = count();
	@symmy[sym(0)] = count();
	@usymmy[usym(0)] = count();
	@funky[func(0)] = count();
	@ufunky[ufunc(0)] = count();
	@moddy[mod(0)] = count();
	@umoddy[umod(0)] = count();
	exit(0);
}
