/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#pragma D depends_on module unix
#pragma D depends_on provider sched

struct cpuinfo {
	processorid_t cpu_id;		/* CPU identifier */
	psetid_t cpu_pset;		/* processor set identifier */
	chipid_t cpu_chip;		/* chip identifier */
	lgrp_id_t cpu_lgrp;		/* locality group identifer */
	processor_info_t cpu_info;	/* CPU information */
};

typedef struct cpuinfo cpuinfo_t;

translator cpuinfo_t < cpu_t *C > {
	cpu_id = C->cpu_id;
	cpu_pset = C->cpu_part->cp_id;
	cpu_chip = C->cpu_physid->cpu_chipid;
	cpu_lgrp = C->cpu_lpl->lpl_lgrpid;
	cpu_info = (processor_info_t)C->cpu_type_info;
}; 

translator cpuinfo_t < disp_t *D > {
	cpu_id = D->disp_cpu == NULL ? -1 :
	    xlate <cpuinfo_t> (D->disp_cpu).cpu_id;
	cpu_pset = D->disp_cpu == NULL ? -1 :
	    xlate <cpuinfo_t> (D->disp_cpu).cpu_pset;
	cpu_chip = D->disp_cpu == NULL ? -1 :
	    xlate <cpuinfo_t> (D->disp_cpu).cpu_chip;
	cpu_lgrp = D->disp_cpu == NULL ? -1 :
	    xlate <cpuinfo_t> (D->disp_cpu).cpu_lgrp;
	cpu_info = D->disp_cpu == NULL ?
	    *((processor_info_t *)dtrace`dtrace_zero) :
	    (processor_info_t)xlate <cpuinfo_t> (D->disp_cpu).cpu_info;
};

inline cpuinfo_t *curcpu = xlate <cpuinfo_t *> (curthread->t_cpu);
#pragma D attributes Stable/Stable/Common curcpu
#pragma D binding "1.0" curcpu

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

