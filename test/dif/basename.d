BEGIN
{
	exit(basename("/foo/bar") == "bar" ? 0 : 1);
}
