/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Verify the nested behavior of unions.
 *
 * SECTION: Structs and Unions/Unions
 *
 */
#pragma D option quiet

union InnerMost {
	int position;
	char content;
};

union InnerMore {
	union InnerMost IMost;
	int dummy_More;
};

union Inner {
	union InnerMore IMore;
	int dummy_More;
};

union Outer {
	union Inner I;
	int dummy_More;
};

union OuterMore {
	union Outer O;
	int dummy_More;
};

union OuterMost {
	union OuterMore OMore;
	int dummy_More;
} OMost;


BEGIN
{

	OMost.OMore.O.I.IMore.IMost.position = 5;
	OMost.OMore.O.I.IMore.IMost.content = 'e';

	printf("OMost.OMore.O.I.IMore.IMost.content: %c\n",
	       OMost.OMore.O.I.IMore.IMost.content);

	exit(0);
}

END
/'e' != OMost.OMore.O.I.IMore.IMost.content/
{
	exit(1);
}
