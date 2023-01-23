/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test a complex structure and verify that tracemem can be used to
 *  look at it.
 *
 * SECTION: Structs and Unions/Structs;
 * 	Actions and Subroutines/tracemem()
 */

#pragma D option quiet

struct s {
	int i;
	char c;
	double d;
	float f;
	long l;
	long long ll;
	union sigval u;
	enum pid_type e;
	struct inode s;
	struct s1 {
		int i;
		char c;
		double d;
		float f;
		long l;
		long long ll;
		union sigval u;
		enum pid_type e;
		struct inode s;
	} sx;
	int a[2];
	int *p;
	int *ap[4];
	int (*fp)();
	int (*afp[2])();
};

BEGIN
{
	tracemem(curthread, sizeof(struct s));
	exit(0);
}
