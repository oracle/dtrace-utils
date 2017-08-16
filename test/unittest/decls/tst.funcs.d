/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION:
 *  Test various kinds of function declarations.
 *
 * SECTION: Program Structure/Probe Clauses and Declarations
 */

extern int getint(void);
extern int *getptr(void);
extern int *(*funcptr)(void);
extern int **(**funcptrptr)(void);

extern int noparms();
extern int oneparm(int);
extern int twoparms(int, int);

BEGIN
{
	exit(0);
}
