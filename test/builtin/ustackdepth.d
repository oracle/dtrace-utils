BEGIN {
	trace(ustackdepth);
	exit(0);
}
