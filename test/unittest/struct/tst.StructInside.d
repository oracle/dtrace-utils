/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * Verify the nested behavior of structs.
 *
 * SECTION: Structs and Unions/Structs
 *
 */

#pragma D option quiet

struct InnerMost {
	int position;
	char content;
};

struct InnerMore {
	struct InnerMost IMost;
	int dummy_More;
};

struct Inner {
	struct InnerMore IMore;
	int dummy_More;
};

struct Outer {
	struct Inner I;
	int dummy_More;
};

struct OuterMore {
	struct Outer O;
	int dummy_More;
};

struct OuterMost {
	struct OuterMore OMore;
	int dummy_More;
} OMost;

struct OuterMost OMostCopy;

BEGIN
{

	OMost.dummy_More = 0;
	OMost.OMore.dummy_More = 1;
	OMost.OMore.O.dummy_More = 2;
	OMost.OMore.O.I.dummy_More = 3;
	OMost.OMore.O.I.IMore.dummy_More = 4;
	OMost.OMore.O.I.IMore.IMost.position = 5;
	OMost.OMore.O.I.IMore.IMost.content = 'e';

	printf("OMost.dummy_More: %d\nOMost.OMore.dummy_More: %d\n",
	OMost.dummy_More, OMost.OMore.dummy_More);

	printf("OMost.OMore.O.dummy_More: %d\n",
	OMost.OMore.O.dummy_More);

	printf("OMost.OMore.O.I.dummy_More: %d\n",
	OMost.OMore.O.I.dummy_More);

	printf("OMost.OMore.O.I.IMore.dummy_More:%d\n",
	OMost.OMore.O.I.IMore.dummy_More);

	printf("OMost.OMore.O.I.IMore.IMost.position: %d\n",
	OMost.OMore.O.I.IMore.IMost.position);

	printf("OMost.OMore.O.I.IMore.IMost.content: %c\n",
	OMost.OMore.O.I.IMore.IMost.content);

	OMostCopy = OMost;

	exit(0);
}

END
/(0 != OMost.dummy_More) || (1 != OMost.OMore.dummy_More) ||
    (2 != OMost.OMore.O.dummy_More) || (3 != OMost.OMore.O.I.dummy_More) ||
    (4 != OMost.OMore.O.I.IMore.dummy_More) ||
    (5 != OMost.OMore.O.I.IMore.IMost.position) ||
    ('e' != OMost.OMore.O.I.IMore.IMost.content)/
{
	exit(1);
}

END
/(0 != OMostCopy.dummy_More) || (1 != OMostCopy.OMore.dummy_More) ||
    (2 != OMostCopy.OMore.O.dummy_More) ||
    (3 != OMostCopy.OMore.O.I.dummy_More) ||
    (4 != OMostCopy.OMore.O.I.IMore.dummy_More) ||
    (5 != OMostCopy.OMore.O.I.IMore.IMost.position) ||
    ('e' != OMostCopy.OMore.O.I.IMore.IMost.content)/
{
	exit(2);
}
