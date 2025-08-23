#!/usr/bin/gawk -f

{
	$1 = $1 > 0x7fffffff ? "OK" : "BAD";
	$3 = $3 > 0x7fffffff ? "OK" : "BAD";
	$5 = $5 > 0x7fffffff ? "OK" : "BAD";
}

{
	print;
}
