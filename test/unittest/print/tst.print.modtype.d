/*
 * Oracle Linux DTrace.
 * Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@nosort */

#pragma D option quiet

BEGIN
{
	tun_page = (struct tun_page *)alloca(sizeof (struct tun_page));
	tun_page->page = (struct page *)0xfeedfacefeedface;
	tun_page->count = 123;
	print(tun_page);
	exit(0);
}
