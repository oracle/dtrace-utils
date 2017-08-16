/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	die = "Die";
	tap = ", SystemTap, ";
	the = "The";
}

BEGIN
{
	phrase = strjoin(die, tap);
	phrase = strjoin(phrase, die);
	expected = "Die, SystemTap, Die";
}

BEGIN
/phrase != expected/
{
	printf("global: expected '%s', found '%s'\n", expected, phrase);
	exit(1);
}

BEGIN
{
	this->phrase = strjoin(the, tap);
}

BEGIN
{
	this->phrase = strjoin(this->phrase, the);
	expected = "The, SystemTap, The";
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
