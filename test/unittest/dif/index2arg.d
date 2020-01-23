/* @@xfail: dtv2 */
BEGIN
{
	exit(index("BEGINNING", "G") == 2 ? 0 : 1);
}
