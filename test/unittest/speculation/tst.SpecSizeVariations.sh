#!/bin/bash

#
# Oracle Linux DTrace.
# Copyright (c) 2024, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace=$1

for x in 71 72 159 160; do
	$dtrace $dt_flags -xspecsize=$x -qn '
	BEGIN
	{
		x = 123456700ll;
		self->nspeculate = 0;
		self->ncommit = 0;
		self->spec = speculation();
		printf("Speculative buffer ID: %d\n", self->spec);
	}

	/* 16 + 7 * 8 = 72 bytes */
	BEGIN
	{
		speculate(self->spec);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		self->nspeculate++;
	}

	BEGIN
	{
		x = 123456800ll;
	}

	/* 16 + 9 * 8 = 88 bytes */
	BEGIN
	{
		speculate(self->spec);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		printf("%lld\n", x++);
		self->nspeculate++;
	}

	BEGIN
	{
		commit(self->spec);
		self->ncommit++;
	}

	BEGIN
	{
		printf("counts: %d %d\n", self->nspeculate, self->ncommit);
		exit(0);
	}

	ERROR
	{
		exit(1);
	}'
done
