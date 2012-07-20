BEGIN {
	trace(curthread);
	exit(0);
}
