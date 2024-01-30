/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: periodic_output */
/* @@nosort */

#pragma D option quiet

fbt:vmlinux:hrtimer_nanosleep:entry /pid == $target/ { func(caller); printf("\n"); }
fbt:vmlinux:hrtimer_nanosleep:entry /pid == $target/ {  mod(caller); printf("\n"); }
fbt:vmlinux:hrtimer_nanosleep:entry /pid == $target/ {  sym(caller); printf("\n"); }
fbt:vmlinux:hrtimer_nanosleep:entry /pid == $target/ { stack(5); }
fbt:vmlinux:hrtimer_nanosleep:entry /pid == $target/ { @['a', func(caller), mod(caller), sym(caller), stack(5), 4] = sum(  34); }
fbt:vmlinux:hrtimer_nanosleep:entry /pid == $target/ { @['a', func(caller), mod(caller), sym(caller), stack(5), 4] = sum(1200); }
fbt:vmlinux:hrtimer_nanosleep:entry /pid == $target/ { exit(0); }
