/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 * A circular definition of structs (two structs defining each other as
 * members) should throw an error at compile time.
 *
 * SECTION: Structs and Unions/Structs
 *
 */


#pragma D option quiet


struct record {
   struct pirate p;
   int position;
   char content;
};


struct pirate {
   struct record r;
   int position;
   char content;
};

struct record rec;
struct pirate pir;

BEGIN
{
	rec.position = 0;
	rec.content = 'a';
	printf("rec.position: %d\nrec.content: %c\n",
	 rec.position, rec.content);

	exit(0);
}

