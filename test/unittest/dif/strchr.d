/* @@xfail: dtv2 */
BEGIN
{
	trace(strchr(probename, 'B'));
	exit(0);
}
