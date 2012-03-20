/* @@xfail: needs CTF */

BEGIN {
	trace(curthread);
	exit(0);
}
