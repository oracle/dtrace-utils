BEGIN
{
	exit(htonl(0x12345678) == 0x78563412 ? 0 : 1);
}
