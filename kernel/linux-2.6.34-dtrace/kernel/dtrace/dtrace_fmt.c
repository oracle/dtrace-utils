/*
 * FILE:	dtrace_fmt.c
 * DESCRIPTION:	Dynamic Tracing: format functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/slab.h>

#include "dtrace.h"

uint16_t dtrace_format_add(dtrace_state_t *state, char *str)
{
	char		*fmt, **new;
	uint16_t	ndx;

	fmt = kstrdup(str, GFP_KERNEL);

	for (ndx = 0; ndx < state->dts_nformats; ndx++) {
		if (state->dts_formats[ndx] == NULL) {
			state->dts_formats[ndx] = fmt;

			return ndx + 1;
		}
	}

	if (state->dts_nformats == UINT16_MAX) {
		kfree(fmt);

		return 0;
	}

	ndx = state->dts_nformats++;
	new = kmalloc((ndx + 1) * sizeof (char *), GFP_KERNEL);

	if (state->dts_formats != NULL) {
		BUG_ON(ndx == 0);
		memcpy(new, state->dts_formats, ndx * sizeof (char *));
		kfree(state->dts_formats);
	}

	state->dts_formats = new;
	state->dts_formats[ndx] = fmt;

	return ndx + 1;
}

void dtrace_format_remove(dtrace_state_t *state, uint16_t format)
{
	char	*fmt;

	BUG_ON(state->dts_formats == NULL);
	BUG_ON(format > state->dts_nformats);
	BUG_ON(state->dts_formats[format - 1] == NULL);

	fmt = state->dts_formats[format - 1];
	kfree(fmt);
	state->dts_formats[format - 1] = NULL;
}
