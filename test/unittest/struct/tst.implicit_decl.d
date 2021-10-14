/*
 * Oracle Linux DTrace.
 * Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Verify assignments when structs are implicitly declared.
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
 * The "dummy" variables are declared just so we can test nonzero offsets.
 */
     struct coords   C_DUMMY, C_DECLD;
this struct coords   c_dummy, c_decld;
     struct particle P_DUMMY, P_DECLD;
this struct particle p_dummy, p_decld;

BEGIN {
	/* Initialize the declared variables. */
	      C_DECLD.x          = 11;
	      C_DECLD.y          = 12;
	      P_DECLD.position.x = 13;
	      P_DECLD.position.y = 14;
	      P_DECLD.velocity.x = 15;
	      P_DECLD.velocity.y = 16;
	this->c_decld.x          = 21;
	this->c_decld.y          = 22;
	this->p_decld.position.x = 23;
	this->p_decld.position.y = 24;
	this->p_decld.velocity.x = 25;
	this->p_decld.velocity.y = 26;

	/* These structs are declared implicitly via their first assignments. */

	      C_IMPL1          =       C_DECLD;
	this->c_impl1          = this->c_decld;
	      C_IMPL2          = this->c_impl1;
	this->c_impl2          =       C_IMPL1;

	      P_IMPL1          =       P_DECLD;
	this->p_impl1          = this->p_decld;
	      P_IMPL2          = this->p_impl1;
	this->p_impl2          =       P_IMPL1;

	      P_IMPL2.velocity = this->c_impl2;
	this->p_impl2.velocity =       C_IMPL2;

	/* print results */

	printf("%d %d %d %d %d %d %d %d\n",
	          P_IMPL2.position.x,
	          P_IMPL2.position.y,
	          P_IMPL2.velocity.x,
	          P_IMPL2.velocity.y,
	    this->p_impl2.position.x,
	    this->p_impl2.position.y,
	    this->p_impl2.velocity.x,
	    this->p_impl2.velocity.y);

	exit(0);
}

ERROR
{
	exit(1);
}
