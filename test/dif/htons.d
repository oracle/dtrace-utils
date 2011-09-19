BEGIN
{
	exit(htons(0x1234) == 0x3412 ? 0 : 1);
}
