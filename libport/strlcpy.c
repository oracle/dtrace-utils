/*
 * Oracle Linux DTrace.
 * Copyright (c) 2008, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <string.h>
#include <sys/types.h>

/*
 * Copies src to the dstsize buffer at dst. The copy will never
 * overflow the destination buffer and the buffer will always be null
 * terminated.
 */

size_t
strlcpy(char *dst, const char *src, size_t len)
{
	size_t slen = strlen(src);
	size_t copied;

	if (len == 0)
		return slen;

	if (slen >= len)
		copied = len - 1;
	else
		copied = slen;
	memcpy(dst, src, copied);
	dst[copied] = '\0';
	return slen;
}
