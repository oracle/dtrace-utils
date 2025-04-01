/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#ifndef _SYS_USDT_NOTE_DEFS_H_
#define	_SYS_USDT_NOTE_DEFS_H_

#define _USDT_SECT_NAME		.note.usdt
#define _USDT_TP_NOTE_NAME	"usdt"		/* (string constant) */
#define _USDT_TP_NOTE_TYPE	1		/* regular probe */
#define _USDT_EN_NOTE_TYPE	2		/* is-enabled probe */
#define _USDT_PV_NOTE_NAME	"prov"		/* (string constant) */
#define _USDT_PV_NOTE_TYPE	1

#endif /* _SYS_USDT_NOTE_DEFS_H_ */
