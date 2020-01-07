/*
 * Oracle Linux DTrace.
 * Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.
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
	ulong_t n = BT_BITOUL(size + 1); /* + 1 for %r0 */
	dt_regset_t *drp = malloc(sizeof (dt_regset_t));

	if (drp == NULL)
		return (NULL);

	drp->dr_bitmap = malloc(sizeof (ulong_t) * n);
	drp->dr_size = size + 1;

	if (drp->dr_bitmap == NULL) {
		dt_regset_destroy(drp);
		return (NULL);
	}

	memset(drp->dr_bitmap, 0, sizeof (ulong_t) * n);
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
	memset(drp->dr_bitmap, 0, sizeof (ulong_t) * BT_BITOUL(drp->dr_size));
}

int
dt_regset_alloc(dt_regset_t *drp)
{
	ulong_t nbits = drp->dr_size - 1;
	ulong_t maxw = nbits >> BT_ULSHIFT;
	ulong_t wx;

	for (wx = maxw; wx >= maxw; wx--) {
		if (drp->dr_bitmap[wx] != ~0UL)
			break;
	}

	if (wx >= 0) {
		ulong_t maxb = (wx == maxw) ? nbits & BT_ULMASK : BT_NBIPUL - 1;
		ulong_t word = drp->dr_bitmap[wx];
		ulong_t bit, bx;
		int reg;

		for (bit = 1 << maxb, bx = maxb; bx >= 0; bx--, bit >>= 1) {
			if ((word & bit) == 0) {
				reg = (int)((wx << BT_ULSHIFT) | bx);
				BT_SET(drp->dr_bitmap, reg);
				return (reg);
			}
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
