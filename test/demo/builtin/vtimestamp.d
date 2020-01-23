/* @@xfail: dtv2 */
BEGIN {
	trace(vtimestamp);
	exit(0);
}
