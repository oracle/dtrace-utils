BEGIN {
	trace(stackdepth);
	exit(0);
}
