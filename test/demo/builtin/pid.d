/* @@xfail: dtv2 */
BEGIN {
	trace(pid);
	exit(0);
}
