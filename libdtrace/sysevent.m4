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
 * Copyright 2007, 2013 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file is an m4 script which is first preprocessed by cpp or cc -E to
 * substitute #define tokens for their values. It is then run over sysevent.d.in
 * to replace those tokens with their values to create the finished sysevent.d.
 */

/*#include <sys/sysevent_impl.h>*/

#define	DEF_REPLACE(x)	__def_replace(#x,x)

DEF_REPLACE(SE_CLASS_NAME)
DEF_REPLACE(SE_SUBCLASS_NAME)
DEF_REPLACE(SE_PUB_NAME)
