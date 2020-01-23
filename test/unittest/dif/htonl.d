/* @@runtest-opts: -C */
/* @@xfail: dtv2 */

#include <endian.h>

BEGIN
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	exit(htonl(0x12345678) == 0x78563412 ? 0 : 1);
#else
	exit(htonl(0x12345678) == 0x12345678 ? 0 : 1);
#endif
}
