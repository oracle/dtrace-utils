BEGIN
{
	exit(cleanpath("/foo/bar/.././../sys/foo") == "/sys/foo" ? 0 : 1);
}
