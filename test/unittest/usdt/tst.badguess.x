#!/bin/bash
# If we can't compile a trivial 32-bit program, this test
# will fail.
echo 'int main (void) { }' | gcc -x c -c -o /dev/null -m32 -
