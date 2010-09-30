/*
 * FILE:	dtrace_buffer.c
 * DESCRIPTION:	Dynamic Tracing: buffer functions
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/slab.h>

#include "dtrace.h"

void dtrace_buffer_free(dtrace_buffer_t *bufs)
{
	int	cpu;

	for_each_online_cpu(cpu) {
		dtrace_buffer_t	*buf = &bufs[cpu];

		if (buf->dtb_tomax == NULL) {
			ASSERT(buf->dtb_xamot == NULL);
			ASSERT(buf->dtb_size == 0);

			continue;
		}

		if (buf->dtb_xamot != NULL) {
			ASSERT(!(buf->dtb_flags & DTRACEBUF_NOSWITCH));

			kfree(buf->dtb_xamot);
		}

		kfree(buf->dtb_tomax);
		buf->dtb_size = 0;
		buf->dtb_tomax = NULL;
	}
}
