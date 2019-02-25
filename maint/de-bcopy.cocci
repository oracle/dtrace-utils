@@
@@
- #include <strings.h>

@@
expression src, dst, n;
@@
- bcopy(src, dst, n)
+ memcpy(dst, src, n)

@@
expression s, n;
@@
- bzero(s, n)
+ memset(s, 0, n)

@@
expression s1, s2, n;
@@
- bcmp(s1, s2, n)
+ memcmp(s1, s2, n)
