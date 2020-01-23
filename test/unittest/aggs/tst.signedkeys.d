/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

/*
 * ASSERTION:
 * 	Signed integer keys print and sort as expected.
 *
 * SECTION: Aggregations, Printing Aggregations
 *
 * NOTES: DTrace sorts integer keys as unsigned values, yet prints 32-
 * and 64-bit integers as signed values. Since the Java DTrace API is
 * expected to emulate this behavior, this test was added to ensure that
 * the behavior is preserved. Consistency with trace() output is also
 * tested.
 */

#pragma D option quiet
#pragma D option aggsortkey

BEGIN
{
	trace((char)-2);
	trace("\n");
	trace((char)-1);
	trace("\n");
	trace((char)0);
	trace("\n");
	trace((char)1);
	trace("\n");
	trace((char)2);
	trace("\n");
	trace("\n");

	trace((short)-2);
	trace("\n");
	trace((short)-1);
	trace("\n");
	trace((short)0);
	trace("\n");
	trace((short)1);
	trace("\n");
	trace((short)2);
	trace("\n");
	trace("\n");

	trace(-2);
	trace("\n");
	trace(-1);
	trace("\n");
	trace(0);
	trace("\n");
	trace(1);
	trace("\n");
	trace(2);
	trace("\n");
	trace("\n");

	trace((long long)-2);
	trace("\n");
	trace((long long)-1);
	trace("\n");
	trace((long long)0);
	trace("\n");
	trace((long long)1);
	trace("\n");
	trace((long long)2);
	trace("\n");

	@i8[(char)-2] = sum(-2);
	@i8[(char)-1] = sum(-1);
	@i8[(char)0] = sum(0);
	@i8[(char)1] = sum(1);
	@i8[(char)2] = sum(2);

	@i16[(short)-2] = sum(-2);
	@i16[(short)-1] = sum(-1);
	@i16[(short)0] = sum(0);
	@i16[(short)1] = sum(1);
	@i16[(short)2] = sum(2);

	@i32[-2] = sum(-2);
	@i32[-1] = sum(-1);
	@i32[0] = sum(0);
	@i32[1] = sum(1);
	@i32[2] = sum(2);

	@i64[(long long)-2] = sum(-2);
	@i64[(long long)-1] = sum(-1);
	@i64[(long long)0] = sum(0);
	@i64[(long long)1] = sum(1);
	@i64[(long long)2] = sum(2);

	exit(0);
}
