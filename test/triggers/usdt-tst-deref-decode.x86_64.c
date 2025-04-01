/*
 * Oracle Linux DTrace.
 * Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#include <sys/sdt.h>

int
main(int argc, char **argv)
{
 int *stp = calloc(100, sizeof(int));
 long i = 0;

 for (i = 0; i < 100; i++)
    stp[i] = (i % 2 == 0) ? i : -i;

 for (;;) {
  /*
   * Using pre-generated macro expansions because we need to force specific
   * expresisons for the probe argument to exercise the decoder.
   */
  do {
    /* extern void __usdt1_test_prov___deref(unsigned long); */
    __asm__ __volatile__ (
      ".altmacro" "\n"
      ".macro _SST x y z" "\n"
      ".iflt \\x" "\n"
      ".ascii \"-\"" "\n"
      ".endif" "\n"
      ".ascii \"\\y\"" "\n"
      ".ifc 8,\\z" "\n"
      ".ascii \"f\"" "\n"
      ".endif" "\n"
      ".ascii \"@\"" "\n"
      ".endm" "\n"
      ".macro SST x" "\n"
      "_SST \\x, %%(-(\\x*((\\x>0)-(\\x<0)))>>8), %%((\\x))&(0xff)" "\n"
      ".endm" "\n"
      "0: nop" "\n"
      ".pushsection .note.usdt,\"?\",@note" "\n"
      ".balign 4" "\n"
      ".4byte 2f-1f" "\n"
      ".4byte 4f-3f" "\n"
      ".4byte 1" "\n"
      "1: .asciz \"usdt\"" "\n"
      "2: .balign 4" "\n"
      "3: .8byte 0b" "\n"
      ".8byte %p[fn]" "\n"
      ".asciz \"test_prov\"" "\n"
      ".asciz \"deref\"" "\n"
      ".byte 10" "\n"
      ".ascii \"-4@4(%%rcx,%%rdx,4) \"" "\n"
      ".ascii \"-4@4(%%rcx,%%rdx) \"" "\n"
      ".ascii \"-8@4(,%%rdx,4) \"" "\n"
      ".ascii \"-4@4(%%rcx) \"" "\n"
      ".ascii \"-4@(%%rcx,%%rdx,4) \"" "\n"
      ".ascii \"-4@(%%rcx,%%rdx) \"" "\n"
      ".ascii \"-8@(,%%rdx,4) \"" "\n"
      ".ascii \"-4@(%%rcx) \"" "\n"
      ".ascii \"-8@%%rcx \"" "\n"
      ".ascii \"-8@%%rdx\"" "\n"
      ".byte 0" "\n"
      "4: .balign 4" "\n"
      ".popsection" "\n"
      ".purgem SST" "\n"
      ".purgem _SST" "\n"
      :: [arr] "c" (stp),
	 [idx] "d" (i),
         [fn] "s" (__func__) );
  } while (0);

  if (++i >= 25)
   i = 0;
 }

 return 0;
}
