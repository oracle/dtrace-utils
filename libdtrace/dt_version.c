/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>
#include <ctype.h>

#include <dt_impl.h>

const dt_version_t _dtrace_versions[] = DTRACE_VERSIONS;

char *
dt_version_num2str(dt_version_t v, char *buf, size_t len)
{
	uint_t	M = DT_VERSION_MAJOR(v);
	uint_t	m = DT_VERSION_MINOR(v);
	uint_t	u = DT_VERSION_MICRO(v);

	if (u == 0)
		snprintf(buf, len, "%u.%u", M, m);
	else
		snprintf(buf, len, "%u.%u.%u", M, m, u);

	return buf;
}

int
dt_version_str2num(const char *s, dt_version_t *vp)
{
	int	i = 0, n[3] = { 0, 0, 0 };
	char	c;

	while ((c = *s++) != '\0') {
		if (isdigit(c))
			n[i] = n[i] * 10 + c - '0';
		else if (c != '.' || i++ >= ARRAY_SIZE(n) - 1)
			return -1;
	}

	if (n[0] > DT_VERSION_MAJMAX ||
	    n[1] > DT_VERSION_MINMAX ||
	    n[2] > DT_VERSION_MICMAX)
		return -1;

	if (vp != NULL)
		*vp = DT_VERSION_NUMBER(n[0], n[1], n[2]);

	return 0;
}

int
dt_version_defined(dt_version_t v)
{
	int	i;

	for (i = 0; i < ARRAY_SIZE(_dtrace_versions); i++) {
		if (_dtrace_versions[i] == v)
			return 1;
	}

	return 0;
}

/*
 * Convert a string representation of a kernel version string into a
 * dt_version_t value.
 */
int
dt_str2kver(const char *kverstr, dt_version_t *vp)
{
	int	kv1, kv2, kv3;
	int	rval;

	rval = sscanf(kverstr, "%d.%d.%d", &kv1, &kv2, &kv3);

	switch (rval) {
	case 2:
		kv3 = 0;
		break;
	case 3:
		break;
	default:
		return -1;
	}

	if (vp)
		*vp = DT_VERSION_NUMBER(kv1, kv2, kv3);

	return 0;
}
