/* @@xfail: dtv2 */
BEGIN {
	trace(caller);
	exit(0);
}
