/*
 * Oracle Linux DTrace.
 * Copyright (c) 2006, 2024, Oracle and/or its affiliates. All rights reserved.
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

	path[i++] = "a/.";
	path[i++] = "a/./.";
	path[i++] = "./a";
	path[i++] = "././a";
	path[i++] = "..";
	path[i++] = "../..";
	path[i++] = "../../..";
	path[i++] = "../../../..";
	path[i++] = "a/..";
	path[i++] =  "abc";
	path[i++] =  "a/";
	path[i++] =  "ab/";
	path[i++] = "/abc";
	path[i++] =  "z/a/../b";
	path[i++] = "/z/a/../b";
	path[i++] =  "z/a/b/../../././c///d/e";
	path[i++] = "/z/a/b/../../././c///d/e";
	path[i++] = "/a/../b";
	path[i++] = "/a/..";
	path[i++] = "/a/../..";
	path[i++] = "/a/b/../../././c///d/e";
	path[i++] =  "a/../b";
	path[i++] =  "a/../..";
	path[i++] =  "a/b/../../././c///d/e";

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
