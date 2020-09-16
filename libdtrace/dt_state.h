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
	DT_STATE_TASK_PARENT_OFF,	/* offsetof(struct task_struct, real_parent) */
	DT_STATE_TASK_TGID_OFF,		/* offsetof(struct task_struct, tgid) */
	DT_STATE_NUM_ELEMS
} dt_state_elem_t;

/*
 * DTrace activity state
 */
typedef enum dt_activity {
	DT_ACTIVITY_INACTIVE = 0,	/* tracing not started */
	DT_ACTIVITY_ACTIVE,		/* tracing in progress */
	DT_ACTIVITY_DRAINING,		/* tracing being stopped */
	DT_ACTIVITY_STOPPED		/* tracing stopped */
} dt_activity_t;

#ifndef __BPF__
# include "dt_bpf.h"
# include "dt_impl.h"

static inline uint32_t
dt_state_get(dtrace_hdl_t *dtp, uint32_t key)
{
	uint32_t        val = 0;

	dt_bpf_map_lookup(dtp->dt_stmap_fd, &key, &val);

	return val;
}

static inline void
dt_state_set(dtrace_hdl_t *dtp, uint32_t key, uint32_t val)
{
	dt_bpf_map_update(dtp->dt_stmap_fd, &key, &val);
}

# define dt_state_get_activity(dtp) \
		((dt_activity_t)dt_state_get(dtp, DT_STATE_ACTIVITY))
# define dt_state_set_activity(dtp, act) \
		dt_state_set(dtp, DT_STATE_ACTIVITY, (uint32_t)(act))

# define dt_state_get_beganon(dtp)	dt_state_get(dtp, DT_STATE_BEGANON)
# define dt_state_get_endedon(dtp)	dt_state_get(dtp, DT_STATE_ENDEDON)

# define dt_state_set_offparent(dtp, x)	dt_state_set(dtp, DT_STATE_TASK_PARENT_OFF, (x))
# define dt_state_set_offtgid(dtp, x)	dt_state_set(dtp, DT_STATE_TASK_TGID_OFF, (x))
#endif

#endif /* _DT_STATE_H */
