#!/usr/bin/gawk -f

/: \/nonexistent\/ld:/ {
	print "sh: /nonexistent/ld: No such file or directory";
	next;
}

{
	print;
}
