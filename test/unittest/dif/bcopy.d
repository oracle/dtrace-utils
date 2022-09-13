/* @@trigger: open */

syscall::open:entry
{
	a = alloca(8);
	bcopy(copyinstr(arg0), a, 8);
	exit(0);
}
