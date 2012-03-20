/* @@xfail: needs CTF */

BEGIN {
	trace(uid);
	exit(0);
}
