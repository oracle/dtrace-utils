/*
 * Oracle Linux DTrace.
 * Copyright © 2012, 2013, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * This file is an m4 script which is first preprocessed by cpp or cc -E to
 * substitute #define tokens for their values. It is then run over procfs.d.in
 * to replace those tokens with their values to create the finished procfs.d.
 */

#define __KERNEL__
#include <linux/sched.h>

#define	DEF_REPLACE(x)	__def_replace(#x,x)

DEF_REPLACE(TASK_RUNNING)
DEF_REPLACE(TASK_INTERRUPTIBLE)
DEF_REPLACE(TASK_UNINTERRUPTIBLE)
DEF_REPLACE(__TASK_STOPPED)
DEF_REPLACE(__TASK_TRACED)
DEF_REPLACE(EXIT_ZOMBIE)
DEF_REPLACE(EXIT_DEAD)
DEF_REPLACE(TASK_DEAD)
DEF_REPLACE(TASK_WAKEKILL)
DEF_REPLACE(TASK_WAKING)
