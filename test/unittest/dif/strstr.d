/* @@xfail: dtv2 */
BEGIN
{
	trace(strstr(probename, "GIN"));
	exit(0);
}
