/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#include "Pcontrol.h"
#include "Putil.h"

/*
 * Place the new element on the list prior to the existing element.
 */
void
list_link(void *new, void *existing)
{
	plist_t *p = new;
	plist_t *q = existing;

	if (q) {
		p->list_forw = q;
		p->list_back = q->list_back;
		q->list_back->list_forw = p;
		q->list_back = p;
	} else {
		p->list_forw = p->list_back = p;
	}
}

/*
 * Unchain the specified element from a list.
 */
void
list_unlink(void *old)
{
	plist_t *p = old;

	if (p->list_forw != p) {
		p->list_back->list_forw = p->list_forw;
		p->list_forw->list_back = p->list_back;
	}
	p->list_forw = p->list_back = p;
}

/*
 * If _libproc_debug is set, printf the debug message to stderr
 * with an appropriate prefix.
 */
/*PRINTFLIKE1*/
_dt_printflike_(1,2)
void
_dprintf(const char *format, ...)
{
	if (_libproc_debug) {
		va_list alist;

		va_start(alist, format);
		(void) fputs("libproc DEBUG: ", stderr);
		(void) vfprintf(stderr, format, alist);
		va_end(alist);
	}
}

/*
 * Printf-style error reporting function.  This is used to supplement the error
 * return codes from various libproc functions with additional text.  Since we
 * are a library, and should not be spewing messages to stderr, we provide a
 * default version of this function that does nothing, but by calling this
 * function we allow the client program to define its own version of the
 * function that will interpose on our empty default.  This may be useful for
 * clients that wish to display such messages to the user.
 */
/*ARGSUSED*/
/*PRINTFLIKE2*/
_dt_printflike_(2,3)
void
Perror_printf(struct ps_prochandle *P _dt_unused_,
	      const char *format _dt_unused_, ...)
{
	/* nothing to do here */
}
