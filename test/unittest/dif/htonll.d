BEGIN
{
	exit(htonll(0x123456780abcdef1) == 0xf1debc0a78563412 ? 0 : 1);
}
