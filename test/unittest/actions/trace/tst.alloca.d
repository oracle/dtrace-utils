#pragma D option quiet

BEGIN
{
	arr = (int *)alloca(5 * sizeof(int));
	idx = 4;
	arr[0] = 1;
	arr[1] = 22;
	arr[2] = 333;
	arr[3] = 4444;
	arr[4] = 55555;
	trace(arr);
	trace(" ");
	trace(*arr);
	trace(" ");
	trace(arr + 2);
	trace(" ");
	trace(*(arr + 2));
	trace(" ");
	trace(arr + idx);
	trace(" ");
	trace(*(arr + idx));
	exit(0);
}
