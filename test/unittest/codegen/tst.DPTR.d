/*
 * Oracle Linux DTrace.
 * Copyright (c) 2022, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * Check the DPTR annotations in the parse tree.
 */

/* @@runtest-opts: -xtree=4 -e */
/* @@nosort */

syscall::write:entry
{
  myvar_int = 4;
  myvar_string = "abcde";
  myvar_alloca = alloca(4);
  myvar_basename = basename("/foo/bar");
  myvar_copyin = copyin(arg1, 4);
  myvar_copyinstr = copyinstr(arg1);
  myvar_dirname = dirname("/foo/bar");
}
