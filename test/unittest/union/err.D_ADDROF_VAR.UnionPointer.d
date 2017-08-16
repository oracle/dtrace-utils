/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Trying to access the members of a user defined union by means of
 * a pointer to it should throw a D_ADDROF_VAR compiler error.
 *
 * SECTION: Structs and Unions/Pointers to Structs
 *
 */

#pragma D option quiet

union record {
	int position;
		  int content;

};

union record var;
union record *ptr;

BEGIN
{

	var.position = 1;
	var.content = 'a';

	ptr = &var;

	printf("ptr->position: %d\tptr->content: %c\n",
	  ptr->position, ptr->content);

	exit(0);
}
