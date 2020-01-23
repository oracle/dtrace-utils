/* @@runtest-opts: -C */
/* @@xfail: dtv2 */

#include <endian.h>

BEGIN
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
	exit(htonll(0x123456780abcdef1) == 0xf1debc0a78563412 ? 0 : 1);
#else
	exit(htonll(0x123456780abcdef1) == 0x123456780abcdef1 ? 0 : 1);
#endif
}
