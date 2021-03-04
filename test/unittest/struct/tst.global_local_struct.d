/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Verify assignments between global and clause-local variables.
 * Assign structs.
 *
 * SECTION: Structs and Unions/Structs
 */

#pragma D option quiet

/*
 * Declare some structs, including a struct of structs.
 */

struct coords {
	int x, y;
};
struct particle {
	struct coords position;
	struct coords velocity;
};

/*
 * Declare some global (uppercase) and clause-local (lowercase) variables.
 * In each trio:
 *   - The first variable is ignored;
 *       it is declared just so we can test nonzero offsets.
 *   - The second variable is initialized.
 *   - The third variable is assigned to.
 */
     int             K, L, M;
this int             k, l, m;
     struct coords   P, Q, R;
this struct coords   p, q, r;
     struct particle A, B, C;
this struct particle a, b, c;

BEGIN {
	/* Initialize the second variables. */
	      L            = 11;
	this->l            = 12;
	      Q.x          = 13;
	      Q.y          = 14;
	this->q.x          = 15;
	this->q.y          = 16;
	      B.position.x = 17;
	      B.position.y = 18;
	      B.velocity.x = 19;
	      B.velocity.y = 20;
	this->b.position.x = 21;
	this->b.position.y = 22;
	this->b.velocity.x = 23;
	this->b.velocity.y = 24;

	printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
	          L           ,
	    this->l           ,
	          Q.x         ,
	          Q.y         ,
	    this->q.x         ,
	    this->q.y         ,
	          B.position.x,
	          B.position.y,
	          B.velocity.x,
	          B.velocity.y,
	    this->b.position.x,
	    this->b.position.y,
	    this->b.velocity.x,
	    this->b.velocity.y);

	/* assign structs */

	      R          = this->q;
	this->r          =       B.velocity;
	      C.velocity = this->b.velocity;
	this->c.velocity =       Q;

	printf("%d %d %d %d %d %d %d %d\n",
	          R.x,
	          R.y,
	    this->r.x,
	    this->r.y,
	          C.velocity.x,
	          C.velocity.y,
	    this->c.velocity.x,
	    this->c.velocity.y);

	exit(0);
}

ERROR
{
	exit(1);
}
