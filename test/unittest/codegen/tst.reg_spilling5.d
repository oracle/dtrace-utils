/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/*
 * ASSERTION: Test that the code generator's spill/fill works with strjoin().
 */
/* @@xfail: DTv2 register management */

#pragma D option quiet

BEGIN
{
  trace(strjoin("abc",
                strjoin("def",
                        strjoin("ghi",
                                strjoin("jkl",
                                        strjoin("mno",
                                                "pqrstuvwx"
                                               )
                                       )
                               )
                       )
               )
       );
  exit(0);
}
