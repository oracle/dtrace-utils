#!/usr/bin/gawk -f

# ignore the banner
/^[ \t]+ID[ \t]+PROVIDER[ \t]+MODULE[ \t]+FUNCTION[ \t]+NAME[ \t]*$/ { next; }

# process other lines
{
	# convert run-dependent PID values to "PID"
	$0 = gensub(/prov([abc]?)[0-9]+/, "prov\\1PID", "g");
	sub(/pid [0-9]+/, "pid PID");

	# convert run-dependent probe ID values to "PRID"
	sub(/^ *[0-9]+/, "PRID");

	# squash blanks
	gsub(/ +/, " ");

	# print
	print;
}
