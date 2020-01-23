/* @@xfail: dtv2 */
BEGIN
{
	self->a = alloca(1024);
	exit(0);
}
