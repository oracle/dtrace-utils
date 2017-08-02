/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * 	Signed integer keys print and sort as expected using the
 * 	aggsortkeypos and aggsortrev options
 *
 * SECTION: Aggregations, Printing Aggregations
 *
 * NOTES: DTrace sorts integer keys as unsigned values, yet prints 32-
 * and 64-bit integers as signed values. Since the Java DTrace API is
 * expected to emulate this behavior, this test was added to ensure that
 * the behavior is preserved.
 */

#pragma D option quiet
#pragma D option aggsortkey
#pragma D option aggsortkeypos=1
#pragma D option aggsortrev

BEGIN
{
	@i8["cat", (char)-2] = sum(-2);
	@i8["dog", (char)-2] = sum(-22);
	@i8["mouse", (char)-2] = sum(-222);
	@i8["cat", (char)-1] = sum(-1);
	@i8["dog", (char)-1] = sum(-11);
	@i8["mouse", (char)-1] = sum(-111);
	@i8["cat", (char)0] = sum(0);
	@i8["dog", (char)0] = sum(10);
	@i8["mouse", (char)0] = sum(100);
	@i8["cat", (char)1] = sum(1);
	@i8["dog", (char)1] = sum(11);
	@i8["mouse", (char)1] = sum(111);
	@i8["cat", (char)2] = sum(2);
	@i8["dog", (char)2] = sum(22);
	@i8["mouse", (char)2] = sum(222);

	@i16["mouse", (short)-2] = sum(-2);
	@i16["dog", (short)-2] = sum(-22);
	@i16["cat", (short)-2] = sum(-222);
	@i16["mouse", (short)-1] = sum(-1);
	@i16["dog", (short)-1] = sum(-11);
	@i16["cat", (short)-1] = sum(-111);
	@i16["mouse", (short)0] = sum(0);
	@i16["dog", (short)0] = sum(10);
	@i16["cat", (short)0] = sum(100);
	@i16["mouse", (short)1] = sum(1);
	@i16["dog", (short)1] = sum(11);
	@i16["cat", (short)1] = sum(111);
	@i16["mouse", (short)2] = sum(2);
	@i16["dog", (short)2] = sum(22);
	@i16["cat", (short)2] = sum(222);

	@i32["mouse", -2] = sum(-2);
	@i32["bear", -2] = sum(-22);
	@i32["cat", -2] = sum(-222);
	@i32["mouse", -1] = sum(-1);
	@i32["bear", -1] = sum(-11);
	@i32["cat", -1] = sum(-111);
	@i32["mouse", 0] = sum(0);
	@i32["bear", 0] = sum(10);
	@i32["cat", 0] = sum(100);
	@i32["mouse", 1] = sum(1);
	@i32["bear", 1] = sum(11);
	@i32["cat", 1] = sum(111);
	@i32["mouse", 2] = sum(2);
	@i32["bear", 2] = sum(22);
	@i32["cat", 2] = sum(222);

	@i64["cat", (long long)-2] = sum(-2);
	@i64["bear", (long long)-2] = sum(-22);
	@i64["dog", (long long)-2] = sum(-222);
	@i64["cat", (long long)-1] = sum(-1);
	@i64["bear", (long long)-1] = sum(-11);
	@i64["dog", (long long)-1] = sum(-111);
	@i64["cat", (long long)0] = sum(0);
	@i64["bear", (long long)0] = sum(10);
	@i64["dog", (long long)0] = sum(100);
	@i64["cat", (long long)1] = sum(1);
	@i64["bear", (long long)1] = sum(11);
	@i64["dog", (long long)1] = sum(111);
	@i64["cat", (long long)2] = sum(2);
	@i64["bear", (long long)2] = sum(22);
	@i64["dog", (long long)2] = sum(222);

	exit(0);
}
