#!/bin/bash
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# If we can't compile a trivial 32-bit program, this test
# will fail.
echo 'int main (void) { }' | gcc -x c -c -o /dev/null -m32 - 2>/dev/null
