/* @@runtest-opts: -C */
/* @@xfail: dtv2 */

#include <endian.h>

BEGIN
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	exit(htons(0x1234) == 0x3412 ? 0 : 1);
#else
	exit(htons(0x1234) == 0x1234 ? 0 : 1);
#endif
}
