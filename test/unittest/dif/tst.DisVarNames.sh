#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2021, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.

# ASSERTION:  That dtrace -S recognizes global and local variable names.

dtrace=$1

# generate disassembly and print the emitted variable names
$dtrace $dt_flags -Sen '
struct foo {
    int foo_i, foo_j, foo_k;
} CheckVariable_A,
  CheckVariable_B,
  CheckVariable_C;

this struct foo
  CheckVariable_a,
  CheckVariable_b,
  CheckVariable_c;

BEGIN {
    CheckVariable_X =
    CheckVariable_Y =
    CheckVariable_Z = 1;

    CheckVariable_A.foo_i =
    CheckVariable_A.foo_j =
    CheckVariable_A.foo_k = 1;

    CheckVariable_B =
    CheckVariable_A;

    CheckVariable_C =
    CheckVariable_A;

    this->CheckVariable_x =
    this->CheckVariable_y =
    this->CheckVariable_z = CheckVariable_Z;

    this->CheckVariable_a =
    this->CheckVariable_b =
    this->CheckVariable_c = CheckVariable_C;

    trace(this->CheckVariable_b.foo_j);
    trace(      CheckVariable_C.foo_k);
    trace(this->CheckVariable_x);
    trace(      CheckVariable_Y);
    trace(this->CheckVariable_z);
}' |& awk '/ ! (|this->)CheckVariable_/ { print $NF }'
