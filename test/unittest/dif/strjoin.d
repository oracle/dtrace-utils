BEGIN
{
	trace(strjoin(probeprov, probename));
	exit(0);
}

ERROR
{
	exit(1);
}
