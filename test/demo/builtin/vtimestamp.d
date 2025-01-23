/* @@xfail: dtv2: need vtimestamp support */

BEGIN {
	trace(vtimestamp);
	exit(0);
}
