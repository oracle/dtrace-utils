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

	s1 = "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~";
	s2 = "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~The quick brown fox jumped over the lazy dog.  Now is the time for all good men to the aid of their party.";

	nerrors += (s1 <= s2 ? 0 : 1);
	nerrors += (s1 <  s2 ? 0 : 1);
	nerrors += (s1 == s2 ? 1 : 0);
	nerrors += (s1 != s2 ? 0 : 1);
	nerrors += (s1 >= s2 ? 1 : 0);
	nerrors += (s1 >  s2 ? 1 : 0);

	nerrors += (s2 <= s1 ? 1 : 0);
	nerrors += (s2 <  s1 ? 1 : 0);
	nerrors += (s2 == s1 ? 1 : 0);
	nerrors += (s2 != s1 ? 0 : 1);
	nerrors += (s2 >= s1 ? 0 : 1);
	nerrors += (s2 >  s1 ? 0 : 1);

	s1 = "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~The quick brown fox jumped over the lazy dog.  Now is the time for all good men to the aid of their party.  To infinity and beyond.";

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
