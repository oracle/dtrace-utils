/* @@xfail: dtv2 */
BEGIN
{
	trace(substr(probename, 2));
	exit(0);
}
