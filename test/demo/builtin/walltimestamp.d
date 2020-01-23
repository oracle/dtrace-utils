/* @@xfail: dtv2 */
BEGIN {
	trace(walltimestamp);
	exit(0);
}
