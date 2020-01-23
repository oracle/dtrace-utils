/* @@xfail: dtv2 */
BEGIN {
	trace(curthread);
	exit(0);
}
