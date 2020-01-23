/* @@xfail: dtv2 */
BEGIN {
	trace(probefunc);
	exit(0);
}
