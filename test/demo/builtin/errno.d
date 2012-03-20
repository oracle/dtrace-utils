BEGIN {
	trace(errno);
	exit(0);
}
