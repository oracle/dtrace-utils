/*
 * Oracle Linux DTrace.
 * Copyright © 2007, 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
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
