#pragma D option destructive

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
