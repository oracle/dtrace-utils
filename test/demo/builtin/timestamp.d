/* @@xfail: dtv2 */
BEGIN {
	trace(timestamp);
	exit(0);
}
