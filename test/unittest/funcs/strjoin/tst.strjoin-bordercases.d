/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option rawbytes
#pragma D option strsize=5
#pragma D option quiet

BEGIN
{
	trace(strjoin("", "123"));
	trace(strjoin("", "12345"));
	trace(strjoin("", "1234567890"));

	trace(strjoin("123", "abcdefgh"));
	trace(strjoin("12345", "abcdefgh"));
	trace(strjoin("1234567890", "abcdefgh"));

	trace(strjoin("123", ""));
	trace(strjoin("12345", ""));
	trace(strjoin("1234567890", ""));

	exit(0);
}

ERROR
{
	exit(1);
}
