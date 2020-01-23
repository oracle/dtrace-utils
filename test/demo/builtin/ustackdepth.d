/* @@xfail: dtv2 */
BEGIN {
	trace(ustackdepth);
	exit(0);
}
