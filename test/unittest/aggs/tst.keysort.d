/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	i = 0;
	j = 0;

	@tour["Ghent", i++, j] = sum(5 - j);
	j++;

	@tour["Berlin", i++, j] = sum(5 - j);
	j++;

	@tour["London", i++, j] = sum(5 - j);
	@tour["Dublin", i++, j] = sum(5 - j);
	j++;

	@tour["Shanghai", i++, j] = sum(5 - j);
	j++;

	@tour["Zurich", i++, j] = sum(5 - j);
	j++;

	@tour["Regina", i++, j] = sum(5 - j);
	@tour["Winnipeg", i++, j] = sum(5 - j);
	@tour["Edmonton", i++, j] = sum(5 - j);
	@tour["Calgary", i++, j] = sum(5 - j);
	@tour["Vancouver", i++, j] = sum(5 - j);
	@tour["Victoria", i++, j] = sum(5 - j);
	j++;

	@tour["Prague", i++, j] = sum(5 - j);
	@tour["London", i++, j] = sum(5 - j);
	j++;

	@tour["Brisbane", i++, j] = sum(5 - j);
	@tour["Sydney", i++, j] = sum(5 - j);
	@tour["Melbourne", i++, j] = sum(5 - j);
	j++;

	setopt("aggsortkey", "false");
	setopt("aggsortkeypos", "0");
	@tour["Amsterdam", i++, j] = sum(5 - j);

	printf("By value:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	setopt("aggsortkey");
	printf("\nBy key, position 0:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	setopt("aggsortkeypos", "1");
	printf("\nBy key, position 1:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	setopt("aggsortkeypos", "2");
	printf("\nBy key, position 2:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	setopt("aggsortkey", "false");
	setopt("aggsortkeypos", "0");
	setopt("aggsortrev");

	printf("\nReversed by value:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	setopt("aggsortkey");
	printf("\nReversed by key, position 0:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	setopt("aggsortkeypos", "1");
	printf("\nReversed by key, position 1:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	setopt("aggsortkeypos", "2");
	printf("\nReversed by key, position 2:\n");
	printa("%20s %8d %8d %8@d\n", @tour);

	exit(0);
}
