#!/usr/bin/awk -f
{
	# ignore the specific probe ID or process ID
	# (the script ensures the process ID is consistent)
	gsub(/[0-9]+/, "NNN");

	# ignore the numbers of spaces for alignment
	# (they depend on the ID widths)
	gsub(/ +/, " ");

	print;
}
