/* @@xfail: Linux has no zones. */

BEGIN {
	trace(zonename);
	exit(0);
}
