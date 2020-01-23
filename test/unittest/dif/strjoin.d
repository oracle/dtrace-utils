/* @@xfail: dtv2 */
BEGIN
{
	trace(strjoin(probeprov, probename));
	exit(0);
}
