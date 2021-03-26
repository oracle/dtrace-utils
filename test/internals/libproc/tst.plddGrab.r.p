#!/bin/sed -f
# Remove the ksplice library that can sometimes be preloaded, and its dep.
/^lib64/libksplice_helper\.so:/d
/^lib64/libdl\.so\.2:/d
s@/lib64/, libksplice_helper\.so, libdl\.so\.2@@g
s,5 libs seen,3 libs seen,
