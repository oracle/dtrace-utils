#pragma D option quiet

BEGIN
{
	a = 1;
	b = 2;
	c = 4;
	d = 8;
	e = 16;
	f = 32;
	g = 64;
	h = 128;
	i = 256;

	/*
	 * Each line is annotated with the register used for the kernel scalar
	 * load.
	 */
	printf("%x\n", `max_pfn); /* %r8 */
	printf("%x\n", a + `max_pfn); /* %r7 */
	printf("%x\n", a + (b + `max_pfn)); /* %r6 */
	printf("%x\n", a + (b + (c + `max_pfn))); /* %r5 */
	printf("%x\n", a + (b + (c + (d + `max_pfn)))); /* %r4 */
	printf("%x\n", a + (b + (c + (d + (e + `max_pfn))))); /* %r3 */
	printf("%x\n", a + (b + (c + (d + (e + (f + `max_pfn)))))); /* %r2 */
	printf("%x\n", a + (b + (c + (d + (e + (f + (g + `max_pfn))))))); /* %r1 */
	printf("%x\n", a + (b + (c + (d + (e + (f + (g + (h + `max_pfn)))))))); /* %r8 */
	printf("%x\n", a + (b + (c + (d + (e + (f + (g + (h + (i + `max_pfn))))))))); /* %r7 */

	exit(0);
}

ERROR
{
	exit(1);
}
