/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: No support for NULL strings yet */

/*
 * ASSERTION: String comparisons work.
 *
 * SECTION:  Operators
 */

#pragma D option quiet

BEGIN
{
	nerrors = 0;

	s1 = "abcdefghi";
	s2 = "jklmnopqr";
	s3 = "stuvwxyz!";

	nerrors += (s1 <= s2 ? 0 : 1);
	nerrors += (s1 <  s2 ? 0 : 1);
	nerrors += (s1 == s2 ? 1 : 0);
	nerrors += (s1 != s2 ? 0 : 1);
	nerrors += (s1 >= s2 ? 1 : 0);
	nerrors += (s1 >  s2 ? 1 : 0);

	nerrors += (s2 <= s2 ? 0 : 1);
	nerrors += (s2 <  s2 ? 1 : 0);
	nerrors += (s2 == s2 ? 0 : 1);
	nerrors += (s2 != s2 ? 1 : 0);
	nerrors += (s2 >= s2 ? 0 : 1);
	nerrors += (s2 >  s2 ? 1 : 0);

	nerrors += (s3 <= s2 ? 1 : 0);
	nerrors += (s3 <  s2 ? 1 : 0);
	nerrors += (s3 == s2 ? 1 : 0);
	nerrors += (s3 != s2 ? 0 : 1);
	nerrors += (s3 >= s2 ? 0 : 1);
	nerrors += (s3 >  s2 ? 0 : 1);

	s2 = NULL;
	nerrors += (s3 <= s2 ? 1 : 0);
	nerrors += (s3 <  s2 ? 1 : 0);
	nerrors += (s3 == s2 ? 1 : 0);
	nerrors += (s3 != s2 ? 0 : 1);
	nerrors += (s3 >= s2 ? 0 : 0);
	nerrors += (s3 >  s2 ? 0 : 0);

	nerrors += (s2 <= s3 ? 0 : 1);
	nerrors += (s2 <  s3 ? 0 : 1);
	nerrors += (s2 == s3 ? 1 : 0);
	nerrors += (s2 != s3 ? 0 : 1);
	nerrors += (s2 >= s3 ? 1 : 0);
	nerrors += (s2 >  s3 ? 1 : 0);

	s3 = NULL;
	nerrors += (s2 <= s3 ? 0 : 1);
	nerrors += (s2 <  s3 ? 1 : 0);
	nerrors += (s2 == s3 ? 0 : 1);
	nerrors += (s2 != s3 ? 1 : 0);
	nerrors += (s2 >= s3 ? 0 : 1);
	nerrors += (s2 >  s3 ? 1 : 0);

	printf("%d errors\n", nerrors);
	exit(nerrors == 0 ? 0 : 1);
}
ERROR
{
	exit(1);
}
