/* @@xfail: dtv2 */
BEGIN {
	exit(probeprov == "dtrace" ? 0 : 1);
}
