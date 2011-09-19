BEGIN {
	trace(caller);
	exit(0);
}
