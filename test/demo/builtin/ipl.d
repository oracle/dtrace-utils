/* @@xfail: dtv2: need ipl support */

BEGIN {
	trace(ipl);
	exit(0);
}
