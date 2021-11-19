BEGIN
{
	exit(dirname("/foo/bar") == "/foo" ? 0 : 1);
}
