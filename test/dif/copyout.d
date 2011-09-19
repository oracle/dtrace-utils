#pragma D option destructive

BEGIN
{
	i = 3;
	copyout((void *)i, 0, 5);
	exit(0);
}

ERROR
{
	exit(1);
}
