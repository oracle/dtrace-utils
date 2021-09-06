/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: String comparisons work.
 *
 * SECTION:  Operators
 */

#pragma D option quiet

BEGIN
{
	nerrors = 0;

	s1 = "abcd\xfa";
	s2 = "abcd\x0a";

	nerrors += (s1 <= s2 ? 1 : 0);
	nerrors += (s1 <  s2 ? 1 : 0);
	nerrors += (s1 == s2 ? 1 : 0);
	nerrors += (s1 != s2 ? 0 : 1);
	nerrors += (s1 >= s2 ? 0 : 1);
	nerrors += (s1 >  s2 ? 0 : 1);

	nerrors += (s2 <= s1 ? 0 : 1);
	nerrors += (s2 <  s1 ? 0 : 1);
	nerrors += (s2 == s1 ? 1 : 0);
	nerrors += (s2 != s1 ? 0 : 1);
	nerrors += (s2 >= s1 ? 1 : 0);
	nerrors += (s2 >  s1 ? 1 : 0);

	s2 = "abcd";

	nerrors += (s1 <= s2 ? 1 : 0);
	nerrors += (s1 <  s2 ? 1 : 0);
	nerrors += (s1 == s2 ? 1 : 0);
	nerrors += (s1 != s2 ? 0 : 1);
	nerrors += (s1 >= s2 ? 0 : 1);
	nerrors += (s1 >  s2 ? 0 : 1);

	nerrors += (s2 <= s1 ? 0 : 1);
	nerrors += (s2 <  s1 ? 0 : 1);
	nerrors += (s2 == s1 ? 1 : 0);
	nerrors += (s2 != s1 ? 0 : 1);
	nerrors += (s2 >= s1 ? 1 : 0);
	nerrors += (s2 >  s1 ? 1 : 0);

	printf("%d errors\n", nerrors);
	exit(nerrors == 0 ? 0 : 1);
}
ERROR
{
	exit(1);
}
