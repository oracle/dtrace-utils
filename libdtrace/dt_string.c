/*
 * Oracle Linux DTrace.
 * Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#include <dt_string.h>

/*
 * Calculate a hash value based on a given string and an initial value.  The
 * initial value is used to calculate compound hash values, e.g.
 *
 *     uint32_t hval;
 *
 *     hval = str2hval(str1, 0);
 *     hval = str2hval(str2, hval);
 */
uint32_t str2hval(const char *p, uint32_t hval)
{
	uint32_t g;

	if (!p)
		return hval;

	while (*p) {
		hval = (hval << 4) + *p++;
		g = hval & 0xf0000000;
		if (g != 0)
			hval ^= g >> 24;

		hval &= ~g;
	}

	return hval;
}

/*
 * Transform string s inline, converting each embedded C escape sequence string
 * to the corresponding character.  For example, the substring "\n" is replaced
 * by an inline '\n' character.  The length of the resulting string is returned.
 */
size_t
stresc2chr(char *s)
{
	char *p, *q, c;
	int esc = 0;
	int x;

	for (p = q = s; (c = *p) != '\0'; p++) {
		if (esc) {
			switch (c) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				c -= '0';
				p++;

				if (*p >= '0' && *p <= '7') {
					c = c * 8 + *p++ - '0';

					if (*p >= '0' && *p <= '7')
						c = c * 8 + *p - '0';
					else
						p--;
				} else
					p--;

				*q++ = c;
				break;

			case 'a':
				*q++ = '\a';
				break;
			case 'b':
				*q++ = '\b';
				break;
			case 'f':
				*q++ = '\f';
				break;
			case 'n':
				*q++ = '\n';
				break;
			case 'r':
				*q++ = '\r';
				break;
			case 't':
				*q++ = '\t';
				break;
			case 'v':
				*q++ = '\v';
				break;

			case 'x':
				for (x = 0; (c = *++p) != '\0'; ) {
					if (c >= '0' && c <= '9')
						x = x * 16 + c - '0';
					else if (c >= 'a' && c <= 'f')
						x = x * 16 + c - 'a' + 10;
					else if (c >= 'A' && c <= 'F')
						x = x * 16 + c - 'A' + 10;
					else
						break;
				}
				*q++ = (char)x;
				p--;
				break;

			case '"':
			case '\\':
				*q++ = c;
				break;
			default:
				*q++ = '\\';
				*q++ = c;
			}

			esc = 0;

		} else {
			if ((esc = c == '\\') == 0)
				*q++ = c;
		}
	}

	*q = '\0';
	return ((size_t)(q - s));
}

/*
 * Create a copy of string s in which certain unprintable or special characters
 * have been converted to the string representation of their C escape sequence.
 * For example, the newline character is expanded to the string "\n".
 */
char *
strchr2esc(const char *s, size_t n)
{
	const char *p;
	char *q, *s2, c;
	size_t addl = 0;

	for (p = s; p < s + n; p++) {
		switch (c = *p) {
		case '\0':
		case '\a':
		case '\b':
		case '\f':
		case '\n':
		case '\r':
		case '\t':
		case '\v':
		case '"':
		case '\\':
			addl++;		/* 1 add'l char needed to follow \ */
			break;
		case ' ':
			break;
		default:
			if (c < '!' || c > '~')
				addl += 3; /* 3 add'l chars following \ */
		}
	}

	if ((s2 = malloc(n + addl + 1)) == NULL)
		return (NULL);

	for (p = s, q = s2; p < s + n; p++) {
		switch (c = *p) {
		case '\0':
			*q++ = '\\';
			*q++ = '0';
			break;
		case '\a':
			*q++ = '\\';
			*q++ = 'a';
			break;
		case '\b':
			*q++ = '\\';
			*q++ = 'b';
			break;
		case '\f':
			*q++ = '\\';
			*q++ = 'f';
			break;
		case '\n':
			*q++ = '\\';
			*q++ = 'n';
			break;
		case '\r':
			*q++ = '\\';
			*q++ = 'r';
			break;
		case '\t':
			*q++ = '\\';
			*q++ = 't';
			break;
		case '\v':
			*q++ = '\\';
			*q++ = 'v';
			break;
		case '"':
			*q++ = '\\';
			*q++ = '"';
			break;
		case '\\':
			*q++ = '\\';
			*q++ = '\\';
			break;
		case ' ':
			*q++ = c;
			break;
		default:
			if (c < '!' || c > '~') {
				*q++ = '\\';
				*q++ = ((c >> 6) & 3) + '0';
				*q++ = ((c >> 3) & 7) + '0';
				*q++ = (c & 7) + '0';
			} else
				*q++ = c;
		}

		if (c == '\0')
			break; /* don't continue past \0 even if p < s + n */
	}

	*q = '\0';
	return (s2);
}

/*
 * Return the basename (name after final /) of the given string.  We use
 * strbasename rather than basename to avoid conflicting with libgen.h's
 * non-const function prototype.
 */
const char *
strbasename(const char *s)
{
	const char *p = strrchr(s, '/');

	if (p == NULL)
		return (s);

	return (++p);
}

/*
 * This function tests a string against the regular expression used for idents
 * and integers in the D lexer, and should match the superset of RGX_IDENT and
 * RGX_INT in dt_lex.l.  If an invalid character is found, the function returns
 * a pointer to it.  Otherwise NULL is returned for a valid string.
 */
const char *
strbadidnum(const char *s)
{
	char *p;
	int c;

	if (*s == '\0')
		return (s);

	errno = 0;
	(void) strtoull(s, &p, 0);

	if (errno == 0 && *p == '\0')
		return (NULL); /* matches RGX_INT */

	while ((c = *s++) != '\0') {
		if (isalnum(c) == 0 && c != '_' && c != '`')
			return (s - 1);
	}

	return (NULL); /* matches RGX_IDENT */
}

/*
 * Determine whether the string contains a glob matching pattern or is just a
 * simple string.  See gmatch(3GEN) and sh(1) for the glob syntax definition.
 */
int
strisglob(const char *s)
{
	char c;

	while ((c = *s++) != '\0') {
		if (c == '[' || c == '?' || c == '*' || c == '\\')
			return (1);
	}

	return (0);
}

/*
 * Hyphenate a string in-place by converting any instances of "__" to "-",
 * which we use for probe names to improve readability, and return the string.
 */
char *
strhyphenate(char *s)
{
	char *p, *q;

	for (p = s, q = p + strlen(p); p < q; p++) {
		if (p[0] == '_' && p[1] == '_') {
			p[0] = '-';
			memmove(p + 1, p + 2, (size_t)(q - p) - 1);
		}
	}

	return (s);
}

/*
 * Search for the last occurrence of the given needle in the given haystack.
 */
char *strrstr(const char *haystack, const char *needle)
{
	const char *s = haystack - 1;
	const char *prev_s = NULL;

	while ((s = strstr(s + 1, needle)) != NULL)
		prev_s = s;

	return (char *)prev_s;
}
