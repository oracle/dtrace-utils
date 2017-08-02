/*
 * Oracle Linux DTrace.
 * Copyright © 2011, 2015, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdio.h>

int
p_online (int cpun)
{
	char online_file[64];
	char online;
	FILE *of;

	snprintf(online_file, sizeof (online_file),
	    "/sys/devices/system/cpu/cpu%i/online", cpun);
	if ((of = fopen(online_file, "r")) == NULL)
		return 0; /* Necessarily online */

	if (fread(&online, 1, 1, of) < 1) {
		fclose(of);
		return 0;
	}
	fclose(of);
	if (online == '1')
		return 0;

	return -1;
}
