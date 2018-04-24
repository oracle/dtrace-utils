/*
 * Oracle Linux DTrace.
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <stdlib.h>
#include "usdt-tst-special-prov.h"

int func;

static void tail_call(void)
{
	TEST_PROV_TAIL();
}

static void enable(void)
{
	if (TEST_PROV_ENABLE_ENABLED())
		TEST_PROV_ENABLE();
}

static void enable_or(void)
{
	if (TEST_PROV_ENABLE_OR_ENABLED() || func > 0)
		TEST_PROV_ENABLE_OR();
}

static void enable_or_2(void)
{
	if (TEST_PROV_TAIL_ENABLED() || TEST_PROV_ENABLE_ENABLED())
		TEST_PROV_ENABLE_OR_2();
}

static void enable_and(void)
{
	if (TEST_PROV_ENABLE_AND_ENABLED() && func > 0)
		TEST_PROV_ENABLE_AND();
}

static void enable_and_2(void)
{
	if (TEST_PROV_TAIL_ENABLED() && TEST_PROV_ENABLE_ENABLED())
		TEST_PROV_ENABLE_AND_2();
}

static void enable_stmt(void)
{
	if (TEST_PROV_ENABLE_STMT_ENABLED())
		TEST_PROV_ENABLE_STMT();
}

static int enable_return(void)
{
	return TEST_PROV_ENABLE_RETURN_ENABLED();
}

int
main(int argc, char **argv)
{
	func = 0;
	if (argc <= 1)
		exit(1);
	func = atoi(argv[1]);

	switch (func) {
	case 1:
		tail_call();
		break;
	case 2:
		enable();
		break;
	case 3:
		enable_or();
		break;
	case 4:
		enable_or_2();
		break;
	case 5:
		enable_and();
		break;
	case 6:
		enable_and_2();
		break;
	case 7:
		enable_stmt();
		break;
	case 8:
		if (enable_return())
			TEST_PROV_ENABLE_RETURN();
		break;
	default:
		exit(2);
	}

	exit(0);
}
