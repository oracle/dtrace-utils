/*
 * Oracle Linux DTrace.
 * Copyright (c) 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D option quiet

BEGIN
{
	this->l1 = strchr("abcdefghij", 'Z');
	this->l2 = strstr("ABCDEFGHIJ", "a");
	this->l3 = this->l1;
}

BEGIN
/this->l1 != this->l2 || this->l1 != this->l2 || this->l1 != this->l3 || this->l1 != NULL/
{
	printf("FAIL\n");
}

BEGIN
{
	printf("done\n");
	exit(0);
}

ERROR
{
	exit(1);
}
