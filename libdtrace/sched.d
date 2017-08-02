/*
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 *
 * Copyright 2012, 2017 Oracle, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma D depends_on module vmlinux

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
