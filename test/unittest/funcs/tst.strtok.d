/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */
/* @@xfail: dtv2 */

#pragma D option quiet

BEGIN
{
	this->str = ",,,Carrots,,Barley,Oatmeal,,,Beans,";
}

BEGIN
/(this->field = strtok(this->str, ",")) == NULL/
{
	exit(1);
}

BEGIN
{
	printf("%s\n", this->field);
}

BEGIN
/(this->field = strtok(NULL, ",")) == NULL/
{
	exit(2);
}

BEGIN
{
	printf("%s\n", this->field);
}

BEGIN
/(this->field = strtok(NULL, ",")) == NULL/
{
	exit(3);
}

BEGIN
{
	printf("%s\n", this->field);
}

BEGIN
/(this->field = strtok(NULL, ",")) == NULL/
{
	exit(4);
}

BEGIN
{
	printf("%s\n", this->field);
}

BEGIN
/(self->a = strtok(NULL, ",")) != NULL/
{
	printf("unexpected field: %s\n", this->field);
	exit(5);
}

struct {
	string s1;
	string s2;
	string result;
} command[int];

int i;

BEGIN
{
	command[i].s1 = "";
	command[i].s2 = "";
	command[i].result = "";
	i++;

	command[i].s1 = "foo";
	command[i].s2 = "";
	command[i].result = command[i].s1;
	i++;

	command[i].s1 = "foobar";
	command[i].s2 = "o";
	command[i].result = "f";
	i++;

	command[i].s1 = "oobar";
	command[i].s2 = "o";
	command[i].result = "bar";
	i++;

	command[i].s1 = "foo";
	command[i].s2 = "bar";
	command[i].result = command[i].s1;
	i++;

	command[i].s1 = "";
	command[i].s2 = "foo";
	command[i].result = "";
	i++;

	end = i;
	i = 0;
}

tick-1ms
/i < end &&
    (this->result = strtok(command[i].s1, command[i].s2)) != command[i].result/
{
	printf("strtok(\"%s\", \"%s\") = \"%s\", expected \"%s\"",
	    command[i].s1, command[i].s2,
	    this->result != NULL ? this->result : "<null>",
	    command[i].result != NULL ? command[i].result : "<null>");
	exit(6 + i);
}

tick-1ms
/++i == end/
{
	exit(0);
}
