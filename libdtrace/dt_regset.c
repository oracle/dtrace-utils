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

#include <stdio.h>

#include <dt_debug.h>
#include <dt_regset.h>

dt_regset_t *
dt_regset_create(ulong_t size, dt_cg_spill_f stf, dt_cg_spill_f ldf)
{
	dt_regset_t *drp = malloc(sizeof(dt_regset_t));

	if (drp == NULL)
		return NULL;

	size++;  /* for %r0 */

	drp->dr_size = size;
	drp->dr_spill_store = stf;
	drp->dr_spill_load = ldf;
	drp->dr_active = malloc(BT_SIZEOFMAP(size));
	drp->dr_spilled = malloc(BT_SIZEOFMAP(size));
	if (drp->dr_active == NULL || drp->dr_spilled == NULL) {
		free(drp->dr_active);
		free(drp->dr_spilled);
		free(drp);
		return NULL;
	}

	dt_regset_reset(drp);
	return drp;
}

void
dt_regset_destroy(dt_regset_t *drp)
{
	free(drp->dr_active);
	free(drp->dr_spilled);
	free(drp);
}

void
dt_regset_reset(dt_regset_t *drp)
{
	memset(drp->dr_active, 0, BT_SIZEOFMAP(drp->dr_size));
	memset(drp->dr_spilled, 0, BT_SIZEOFMAP(drp->dr_size));
}

int
dt_regset_alloc(dt_regset_t *drp)
{
	int reg;

	for (reg = drp->dr_size - 1; reg >= 0; reg--) {
		if (BT_TEST(drp->dr_active, reg) == 0) {
			BT_SET(drp->dr_active, reg);
			return reg;
		}
	}

	for (reg = drp->dr_size - 1; reg >= 0; reg--) {
		if (BT_TEST(drp->dr_spilled, reg) == 0) {
			drp->dr_spill_store(reg);
			BT_SET(drp->dr_spilled, reg);
			return reg;
		}
	}

	return -1;			/* no available registers */
}

/*
 * Allocate a specific register.
 */
int
dt_regset_xalloc(dt_regset_t *drp, int reg)
{
	assert(reg >= 0 && reg < drp->dr_size);
	if (BT_TEST(drp->dr_active, reg) != 0) {
		if (BT_TEST(drp->dr_spilled, reg) != 0)
			return -1;	/* register in use (and spilled)*/

		drp->dr_spill_store(reg);
		BT_SET(drp->dr_spilled, reg);
	}

	BT_SET(drp->dr_active, reg);

	return 0;
}

void
dt_regset_free(dt_regset_t *drp, int reg)
{
	assert(reg >= 0 && reg < drp->dr_size);
	assert(BT_TEST(drp->dr_active, reg) != 0);

	if (BT_TEST(drp->dr_spilled, reg) != 0) {
		drp->dr_spill_load(reg);
		BT_CLEAR(drp->dr_spilled, reg);
		return;
	}

	BT_CLEAR(drp->dr_active, reg);
}

/*
 * Allocate %r1 through %r5 for use as function call arguments in BPF.
 */
int
dt_regset_xalloc_args(dt_regset_t *drp)
{
	int	reg;

	for (reg = 1; reg <= 5; reg++) {
		if (dt_regset_xalloc(drp, reg) == -1) {
			while (--reg > 0)
				dt_regset_free(drp, reg);

			return -1;	/* register in use */
		}
	}

	return 0;
}

/*
 * Free %r1 through %r5 after they were used as function call arguments in BPF.
 */
void
dt_regset_free_args(dt_regset_t *drp)
{
	int	reg;

	for (reg = 1; reg <= 5; reg++)
		dt_regset_free(drp, reg);
}

/*
 * Dump the current register allocation.
 */
void
dt_regset_dump(dt_regset_t *drp, const char *pref)
{
	int reg;

	fprintf(stderr, "%s: Regset: ", pref);
	for (reg = 0; reg < drp->dr_size; reg++) {
		fprintf(stderr, "%c", BT_TEST(drp->dr_active, reg) ? 'x' :
				      BT_TEST(drp->dr_spilled, reg) ? 's' :
				      '.');
	}
	fprintf(stderr, "\n");
}
