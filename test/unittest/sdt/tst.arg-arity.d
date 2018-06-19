/*
 * Oracle Linux DTrace.
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

/* @@trigger: testprobe */

sdt:dt_test::sdt-test {}
sdt:dt_test::sdt-test-arg1 { trace(args[0]); }
sdt:dt_test::sdt-test-arg2 { trace(args[0]); trace(args[1]); }
sdt:dt_test::sdt-test-arg3 { trace(args[0]); trace(args[1]); trace(args[2]); }
sdt:dt_test::sdt-test-arg4 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); }
sdt:dt_test::sdt-test-arg5 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); }
sdt:dt_test::sdt-test-arg6 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); }
sdt:dt_test::sdt-test-arg7 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); }
sdt:dt_test::sdt-test-arg8 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); }
sdt:dt_test::sdt-test-arg9 { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]);}
sdt:dt_test::sdt-test-arga { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); }
sdt:dt_test::sdt-test-argb { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); }
sdt:dt_test::sdt-test-argc { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); trace(args[11]); }
sdt:dt_test::sdt-test-argd { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); trace(args[11]); trace(args[12]); }
sdt:dt_test::sdt-test-arge { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); trace(args[11]); trace(args[12]); trace(args[13]); }
sdt:dt_test::sdt-test-argf { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); trace(args[11]); trace(args[12]); trace(args[13]); trace(args[14]); }
sdt:dt_test::sdt-test-argg { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); trace(args[11]); trace(args[12]); trace(args[13]); trace(args[14]); trace(args[15]); }
sdt:dt_test::sdt-test-argh { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); trace(args[11]); trace(args[12]); trace(args[13]); trace(args[14]); trace(args[15]); trace(args[16]); }
sdt:dt_test::sdt-test-argi { trace(args[0]); trace(args[1]); trace(args[2]); trace(args[3]); trace(args[4]); trace(args[5]); trace(args[6]); trace(args[7]); trace(args[8]); trace(args[9]); trace(args[10]); trace(args[11]); trace(args[12]); trace(args[13]); trace(args[14]); trace(args[15]); trace(args[16]); trace(args[17]); }

syscall::exit_group:entry
/execname == "testprobe"/
{
        exit(0);
}
