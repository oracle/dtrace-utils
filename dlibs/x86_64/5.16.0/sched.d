/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D depends_on module vmlinux

typedef id_t		processorid_t;
typedef id_t		psetid_t;
typedef id_t		chipid_t;
typedef id_t		lgrp_id_t;

typedef struct cpuinfo {
	processorid_t	cpu_id;
	psetid_t	cpu_pset;
	chipid_t	cpu_chip;
	lgrp_id_t	cpu_lgrp;
} cpuinfo_t;

typedef cpuinfo_t	*cpuinfo_t_p;

inline processorid_t cpu = curcpu->cpu_id;
#pragma D attributes Stable/Stable/Common cpu
#pragma D binding "1.0" cpu

inline psetid_t pset = curcpu->cpu_pset;
#pragma D attributes Stable/Stable/Common pset
#pragma D binding "1.0" pset

inline chipid_t chip = curcpu->cpu_chip;
#pragma D attributes Stable/Stable/Common chip
#pragma D binding "1.0" chip

inline lgrp_id_t lgrp = curcpu->cpu_lgrp;
#pragma D attributes Stable/Stable/Common lgrp
#pragma D binding "1.0" lgrp
