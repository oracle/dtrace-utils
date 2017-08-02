/*
 * Oracle Linux DTrace.
 * Copyright © 2006, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	@[8] = sum(1);
	@[6] = sum(1);
	@[7] = sum(1);
	@[5] = sum(1);
	@[3] = sum(1);
	@[0] = sum(1);
	@[9] = sum(1);

	@tour["Ghent"] = sum(1);
	@tour["Berlin"] = sum(1);
	@tour["London"] = sum(1);
	@tour["Dublin"] = sum(1);
	@tour["Shanghai"] = sum(1);
	@tour["Zurich"] = sum(1);
	@tour["Regina"] = sum(1);
	@tour["Winnipeg"] = sum(1);
	@tour["Edmonton"] = sum(1);
	@tour["Calgary"] = sum(1);

	@ate[8, "Rice"] = sum(1);
	@ate[8, "Oatmeal"] = sum(1);
	@ate[8, "Barley"] = sum(1);
	@ate[8, "Carrots"] = sum(1);
	@ate[8, "Sweet potato"] = sum(1);
	@ate[8, "Asparagus"] = sum(1);
	@ate[8, "Squash"] = sum(1);

	@chars['a'] = sum(1);
	@chars['s'] = sum(1);
	@chars['d'] = sum(1);
	@chars['f'] = sum(1);

	printa("%d\n", @);
	printf("\n");

	printa("%s\n", @tour);
	printf("\n");

	printa("%d %s\n", @ate);
	printf("\n");

	printa("%c\n", @chars);
	printf("\n");

	exit(0);
}
