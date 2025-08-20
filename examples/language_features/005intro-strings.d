#!/usr/sbin/dtrace -s

/*
 *  SYNOPSIS
 *    sudo ./005intro-strings.d
 *
 *  DESCRIPTION
 *    But D does differ from C when it comes to strings.
 *    The D language has a built-in string data type.
 */

/*
 * Strings are stored in arrays of fixed length.  The
 * default length 256 can be overridden with a D option.
 */
#pragma D option strsize=12

/* string variables can be declared explicitly */
string exp;
string s1, s2, s3, s4;

char carray[32];

BEGIN
{
	/* strings are truncated, if necessary, per strsize */
	printf("%s\n", "abcdefghijklmnopqrstuvwxyz");

	/* string variables can be declared implicitly */
	imp = "abcdefghijklmnopqrstuvwxyz";
	printf("%d chars, string %s\n", sizeof(imp), imp);

	/* operators can be used to order strings lexically (akin to strcmp) */
	exp = "zyxwvutsrqponmlkjihgfedcba";
	printf("%s is %s than %s\n", imp, imp < exp ? "less" : "greater", exp);

	/* set up NULL-terminated char array for some illustration purposes */
	carray[ 0] = 'A'; carray[ 1] = 'B'; carray[ 2] = 'C'; carray[ 3] = 'D';
	carray[ 4] = 'E'; carray[ 5] = 'F'; carray[ 6] = 'G'; carray[ 7] = 'H';
	carray[ 8] = 'I'; carray[ 9] = 'J'; carray[10] = 'K'; carray[11] = 'L';
	carray[12] = 'M'; carray[13] = 'N'; carray[14] = 'O'; carray[15] = 'P';
	carray[16] = 'Q'; carray[17] = 'R'; carray[18] = 'S'; carray[19] = 'T';
	carray[20] = 'U'; carray[21] = 'V'; carray[22] = 'W'; carray[23] = 'X';
	carray[24] = 'Y'; carray[25] = 'Z'; carray[26] = '\0';

	/* char pointers can be turned into strings */
	s1 = (string) carray;	/* type casting */
	s2 = stringof carray;	/* stringof operator */

	/* char pointers can be promoted automatically to strings */
	s3 = carray;
	s4 = strchr(carray, 'B');

	/*
	 * Note that in the above assignments, we did not simply set
	 * a char* to some existing string buffer.  Rather, contents
	 * are copied "by-value" to the destination string buffer.
	 * E.g., if the source buffer is modified, the output strings
	 * nevertheless all stay the same.
	 */
	carray[2] = '\0';
	printf("%s %s %s %s %s\n", s1, s2, s3, s4, carray);

	exit(0);
}
