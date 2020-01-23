/* @@xfail: dtv2 */
BEGIN {
	trace(stackdepth);
	exit(0);
}
