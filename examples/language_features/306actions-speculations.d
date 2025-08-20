#!/usr/sbin/dtrace -s

#pragma D option quiet

/*
 *  SYNOPSIS
 *    sudo ./306actions-speculations.d
 *
 *  DESCRIPTION
 *    In DTrace, one can write output speculatively, deciding at a
 *    later time whether to commit or discard the speculative output.
 */
#pragma D option nspec=2

BEGIN
{
	/* create two speculations */
	s1 = speculation();
	s2 = speculation();
}

BEGIN
{
	/* the clause has "speculate()" before data recording */
	speculate(s1);
	printf("one\n");
}

BEGIN
{
	speculate(s2);
	printf("two\n");
}

BEGIN
{
	speculate(s1);
	printf("three\n");
}

BEGIN
{
	speculate(s2);
	printf("four\n");
}

BEGIN
{
	commit(s1);  /* commit "one" "three" output */
	discard(s2);  /* discard "two" "four" output */
}

BEGIN
{
	/* cannot be in same clause as commit/discard */
	exit(0);
}
