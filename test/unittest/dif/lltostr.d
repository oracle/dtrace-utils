/* @@xfail: dtv2 */
BEGIN
{
	exit(lltostr(12345678) == "12345678" ? 0 : 1);
}
