/* @@xfail: dtv2 */
BEGIN
{
	trace(substr(probename, 0, 2));
	exit(0);
}
