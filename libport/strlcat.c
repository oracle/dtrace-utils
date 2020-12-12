/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <string.h>
#include <sys/types.h>

/*
 * Appends src to the dstsize buffer at dst. The append will never
 * overflow the destination buffer and the buffer will always be null
 * terminated. Never reference beyond &dst[dstsize-1] when computing
 * the length of the pre-existing string.
 */

size_t
strlcat(char *dst, const char *src, size_t dstsize)
{
	char *df = dst;
	size_t left = dstsize;
	size_t l1;
	size_t l2 = strlen(src);
	size_t copied;

	while (left-- != 0 && *df != '\0')
		df++;
	l1 = df - dst;
	if (dstsize == l1)
		return l1 + l2;

	copied = l1 + l2 >= dstsize ? dstsize - l1 - 1 : l2;
	memcpy(dst + l1, src, copied);
	dst[l1+copied] = '\0';
	return l1 + l2;
}
