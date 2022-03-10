/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

/*
 * This test verifies that the basename() and dirname() functions are working
 * properly.  Note that the output of this is a bash script.  When run,
 * it will give no output if the output is correct.
 */
BEGIN
{
	dir[i++] = "/foo/bar/baz";
	dir[i++] = "/foo/bar///baz/";
	dir[i++] = "/foo/bar/baz/";
	dir[i++] = "/foo/bar/baz//";
	dir[i++] = "/foo/bar/baz/.";
	dir[i++] = "/foo/bar/baz/./";
	dir[i++] = "/foo/bar/baz/.//";
	dir[i++] = "foo/bar/baz/";
	dir[i++] = "/";
	dir[i++] = "./";
	dir[i++] = "//";
	dir[i++] = "/.";
	dir[i++] = "/./";
	dir[i++] = "/./.";
	dir[i++] = "/.//";
	dir[i++] = ".";
	dir[i++] = "f";
	dir[i++] = "f/";
	dir[i++] = "/////";
	dir[i++] = "";

	end = i;
	i = 0;

	printf("#!/bin/bash\n\n");
}

tick-1ms
/i < end/
{
	printf("if [ `basename \"%s\"` != \"%s\" ]; then\n",
	    dir[i], basename(dir[i]));
	printf("	echo \"basename(\\\"%s\\\") is \\\"%s\\\"; ",
	    dir[i], basename(dir[i]));
	printf("expected \\\"`basename \"%s\"`\"\\\"\n", dir[i]);
	printf("fi\n\n");
	printf("if [ `dirname \"%s\"` != \"%s\" ]; then\n",
	    dir[i], dirname(dir[i]));
	printf("	echo \"dirname(\\\"%s\\\") is \\\"%s\\\"; ",
	    dir[i], dirname(dir[i]));
	printf("expected \\\"`dirname \"%s\"`\"\\\"\n", dir[i]);
	printf("fi\n\n");
	i++;
}

tick-1ms
/i == end/
{
	exit(0);
}
