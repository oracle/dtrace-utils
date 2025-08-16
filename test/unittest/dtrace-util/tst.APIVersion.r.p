#!/usr/bin/gawk -f

# We do not care about the actual version number - we just want to ensure that
# a version using the correct format is reported.
/dtrace:/ {
	sub(/[1-9][0-9]*\.[0-9]+\.[0-9]+/, "x.y.z");
	sub(/[1-9][0-9]*\.[0-9]+/, "x.y.z");
}

{
	print;
}
