/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	lv = "Live";
	dt = ", DTrace, ";
	ps = "Prosper";
}

BEGIN
{
	phrase = strjoin(lv, dt);
	phrase = strjoin(phrase, lv);
	expected = "Live, DTrace, Live";
}

BEGIN
/phrase != expected/
{
	printf("global: expected '%s', found '%s'\n", expected, phrase);
	exit(1);
}

BEGIN
{
	this->phrase = strjoin(ps, dt);
}

BEGIN
{
	this->phrase = strjoin(this->phrase, ps);
	expected = "Prosper, DTrace, Prosper";
}

BEGIN
/this->phrase != expected/
{
	printf("clause-local: expected '%s', found '%s'\n",
	    expected, this->phrase);
	exit(2);
}

BEGIN
{
	phrase = NULL;
	this->phrase = NULL;
}

BEGIN
/phrase != NULL/
{
	printf("expected global to be NULL\n");
	exit(3);
}

BEGIN
/this->phrase != NULL/
{
	printf("expected clause-local to be NULL\n");
	exit(4);
}

BEGIN
{
	exit(0);
}
