/*
 * Oracle Linux DTrace.
 * Copyright © 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Translated data pointers cannot be cast to uintptr_t.
 *
 * SECTION: Translators
 */

#pragma D option quiet

struct in {
	int ii;
};

struct out {
	int oi;
};

translator struct out < struct in *i >
{
	oi = i->ii;

};

struct in *f;

BEGIN
{
	f->ii = 1;
	addr = (uintptr_t)xlate<struct out *>(f)
}
