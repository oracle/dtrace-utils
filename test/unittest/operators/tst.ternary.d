/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test the ternary operator.  Test left-hand side true, right-hand side true,
 *  and multiple nested instances of the ternary operator.  Since inet_ntoa*()
 *  relies on temporary strings, and ternary operators need both left- and
 *  right-side temporaries, test ternary inet_ntoa*() also.
 *
 *
 * SECTION:  Types, Operators, and Expressions/Conditional Expressions
 */

#pragma D option quiet

BEGIN
{
	x = 0;
	y = 1;
	printf("x is %s\n", x == 0 ? "zero" : "one");
	x = 1;
	printf("x is %s\n", x == 0 ? "zero" : "one");
	x = 2;
	printf("x is %s\n", x == 0 ? "zero" : x == 1 ? "one" : "two");
	ipaddr = (ipaddr_t *)alloca(sizeof(ipaddr_t));
	ipaddr2 = (in6_addr_t *)alloca(sizeof(in6_addr_t));
	ipaddr2->in6_u.u6_addr32[0] = 0xffffffff;
	ipaddr2->in6_u.u6_addr32[1] = 0xffffffff;
	ipaddr2->in6_u.u6_addr32[2] = 0xffffffff;
	ipaddr2->in6_u.u6_addr32[3] = 0xffffffff;
	printf("ipaddr is %s\n", x > 1 ? inet_ntoa(ipaddr) :
				 x > 2 ? inet_ntoa(ipaddr) :
				 x > 3 ? inet_ntoa(ipaddr) :
				 inet_ntoa(ipaddr));
	printf("ipaddr2 is %s\n", x > 1 ? inet_ntoa6(ipaddr2) :
				  x > 2 ? inet_ntoa6(ipaddr2) :
				  x > 3 ? inet_ntoa6(ipaddr2) :
				  inet_ntoa6(ipaddr2));
	exit(0);
}
