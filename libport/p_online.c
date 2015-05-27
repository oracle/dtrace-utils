/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2011, 2015 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
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
