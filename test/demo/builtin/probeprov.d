BEGIN {
	exit(probeprov == "dtrace" ? 0 : 1);
}
