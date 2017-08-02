/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	path[i++] = "/foo/bar/baz";
	path[i++] = "/foo/bar///baz/";
	path[i++] = "/foo/bar/baz/";
	path[i++] = "/foo/bar/baz//";
	path[i++] = "/foo/bar/baz/.";
	path[i++] = "/foo/bar/baz/./";
	path[i++] = "/foo/bar/../../baz/.//";
	path[i++] = "foo/bar/./././././baz/";
	path[i++] = "/foo/bar/baz/../../../../../../";
	path[i++] = "/../../../../../../";
	path[i++] = "/./";
	path[i++] = "/foo/bar/baz/../../bop/bang/../../bar/baz/";
	path[i++] = "./";
	path[i++] = "//";
	path[i++] = "/.";
	path[i++] = "/./";
	path[i++] = "/./.";
	path[i++] = "/.//";
	path[i++] = ".";
	path[i++] = "/////";
	path[i++] = "";

	end = i;
	i = 0;
}

tick-1ms
/i < end/
{
	printf("cleanpath(\"%s\") = \"%s\"\n", path[i], cleanpath(path[i]));
	i++;
}

tick-1ms
/i == end/
{
	exit(0);
}
