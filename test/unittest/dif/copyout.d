#pragma D option destructive
/* @@xfail: dtv2 */

BEGIN
{
	i = 3;
	copyout((void *)i, 0, 5);
	exit(1);
}

ERROR
{
	exit(0);
}
