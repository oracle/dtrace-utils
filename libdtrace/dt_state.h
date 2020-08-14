/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
 */

#ifndef _DT_STATE_H
#define _DT_STATE_H

/*
 * DTrace 'state' BPF map.
 *
 * The keys are uint32_t values corresponding to dt_state_elem_t values.
 * The values are uint32_t values.
 */
#define DT_STATE_KEY_TYPE	uint32_t
#define DT_STATE_VAL_TYPE	uint32_t

typedef enum dt_state_elem {
	DT_STATE_ACTIVITY = 0,		/* activity state of the session */
	DT_STATE_BEGANON,		/* cpu BEGIN probe executed on */
	DT_STATE_ENDEDON,		/* cpu END probe executed on */
	DT_STATE_NUM_ELEMS
} dt_state_elem_t;

#endif /* _DT_STATE_H */
