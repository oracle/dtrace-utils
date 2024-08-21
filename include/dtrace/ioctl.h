/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2009, 2024, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef _DTRACE_IOCTL_H_
#define _DTRACE_IOCTL_H_

#include <linux/ioctl.h>
#include <dtrace/helpers.h>

#define DTRACEHIOC		0xd8
#define DTRACEHIOC_ADD		_IOW(DTRACEHIOC, 1, dof_hdr_t)
#define DTRACEHIOC_REMOVE	_IOW(DTRACEHIOC, 2, int)
#define DTRACEHIOC_ADDDOF	_IOW(DTRACEHIOC, 3, dof_helper_t)

#endif /* _DTRACE_IOCTL_H */
