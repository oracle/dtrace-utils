/* @@xfail: needs CTF */

BEGIN {
	trace(gid);
	exit(0);
}
