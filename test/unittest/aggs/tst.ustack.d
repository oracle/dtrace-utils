/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: ustack-tst-basic */
/* @@nosort */

#pragma D option quiet

profile-1 /pid == $target/ { uaddr(ucaller); printf("\n"); }
profile-1 /pid == $target/ { ufunc(ucaller); printf("\n"); }
profile-1 /pid == $target/ {  umod(ucaller); printf("\n"); }
profile-1 /pid == $target/ {  usym(ucaller); printf("\n"); }
profile-1 /pid == $target/ { ustack(5); }
profile-1 /pid == $target/ { @['a', uaddr(ucaller), ufunc(ucaller), umod(ucaller), usym(ucaller), ustack(5), 4] = sum(  34); }
profile-1 /pid == $target/ { @['a', uaddr(ucaller), ufunc(ucaller), umod(ucaller), usym(ucaller), ustack(5), 4] = sum(1200); }
profile-1 /pid == $target/ { exit(0); }
