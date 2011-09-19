# @@xfail: BEGIN probe has no arguments

BEGIN {
	printf("0=%u 1=%u 2=%u 3=%u 4=%u 5=%u 6=%u 7=%u 8=%u 9=%u\n",
	       args[0], args[1], args[2], args[3], args[4], args[5], args[6],
	       args[7], args[8], args[9]);
	exit(0);
}
