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

BEGIN {
	trace
	(strjoin("abc", "def") > strjoin("def", "abc") ? 1 :
	(strjoin("ABC", "def") > strjoin("def", "ABC") ? 2 :
	(strjoin("abc", "DEF") > strjoin("DEF", "abc") ? 3 :
	(strjoin("ABC", "DEF") > strjoin("DEF", "ABC") ? 4 :
	(strjoin("abc", "deF") > strjoin("deF", "abc") ? 5 :
	(strjoin("ABC", "deF") > strjoin("deF", "ABC") ? 6 :
	(strjoin("abc", "DEf") > strjoin("DEf", "abc") ? 7 :
	(strjoin("ABC", "DEf") > strjoin("DEf", "ABC") ? 8 :
	(strjoin("abC", "def") > strjoin("def", "abC") ? 9 :
	(strjoin("ABc", "def") > strjoin("def", "ABc") ? 10 :
	(strjoin("abC", "DEF") > strjoin("DEF", "abC") ? 11 :
	(strjoin("ABc", "DEF") > strjoin("DEF", "ABc") ? 12 :
	(strjoin("abC", "deF") > strjoin("deF", "abC") ? 13 :
	(strjoin("ABc", "deF") > strjoin("deF", "ABc") ? 14 :
	(strjoin("abC", "DEf") > strjoin("DEf", "abC") ? 15 :
	(strjoin("ABc", "DEf") > strjoin("DEf", "ABc") ? 16 : 0))))))))))))))));
	exit(0);
}
ERROR
{
	exit(1);
}
