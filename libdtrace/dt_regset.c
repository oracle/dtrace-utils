/*
 * Oracle Linux DTrace.
 * Copyright (c) 2003, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <sys/types.h>
#include <sys/bitmap.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <dt_debug.h>
#include <dt_regset.h>

dt_regset_t *
dt_regset_create(ulong_t size)
{
	dt_regset_t *drp = malloc(sizeof (dt_regset_t));

	if (drp == NULL)
		return (NULL);

	size++;  /* for %r0 */

	drp->dr_size = size;
	drp->dr_bitmap = malloc(BT_SIZEOFMAP(size));
	if (drp->dr_bitmap == NULL) {
		free(drp);
		return (NULL);
	}

	dt_regset_reset(drp);
	return (drp);
}

void
dt_regset_destroy(dt_regset_t *drp)
{
	free(drp->dr_bitmap);
	free(drp);
}

void
dt_regset_reset(dt_regset_t *drp)
{
	memset(drp->dr_bitmap, 0, BT_SIZEOFMAP(drp->dr_size));
}

int
dt_regset_alloc(dt_regset_t *drp)
{
	int reg;

	for (reg = drp->dr_size - 1; reg >= 0; reg--) {
		if (BT_TEST(drp->dr_bitmap, reg) == 0) {
			BT_SET(drp->dr_bitmap, reg);
			return (reg);
		}
	}
	return (-1); /* no available registers */
}

void
dt_regset_xalloc(dt_regset_t *drp, int reg)
{
	assert(reg >= 0 && reg < drp->dr_size);
	assert(BT_TEST(drp->dr_bitmap, reg) == 0);

	BT_SET(drp->dr_bitmap, reg);
}

void
dt_regset_free(dt_regset_t *drp, int reg)
{
	assert(reg >= 0 && reg < drp->dr_size);
	assert(BT_TEST(drp->dr_bitmap, reg) != 0);
	BT_CLEAR(drp->dr_bitmap, reg);
}
