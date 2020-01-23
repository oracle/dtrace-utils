/* @@xfail: dtv2 */
BEGIN {
	exit(probename == "BEGIN" ? 0 : 1);
}
