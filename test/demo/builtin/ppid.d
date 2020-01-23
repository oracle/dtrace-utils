/* @@xfail: dtv2 */
BEGIN {
	trace(ppid);
	exit(0);
}
