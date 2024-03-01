/*
 * Oracle Linux DTrace.
 * Copyright (c) 2020, 2024, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <dt_impl.h>

#define SYSFS_CPU	"/sys/devices/system/cpu/"
#define	ONLINE_CPUS	SYSFS_CPU "online"
#define	POSSIBLE_CPUS	SYSFS_CPU "possible"
#define	CPU_CHIP_ID	SYSFS_CPU "cpu%d/topology/physical_package_id"

static uint32_t
dt_cpuinfo_read(dtrace_hdl_t *dtp, const char *fn, cpuinfo_t **cip)
{
	FILE		*fp;
	int		cnt = 0;
	cpuinfo_t	*ci = NULL;

	uint32_t	num, start = 0;
	int		range = 0;
	char		c[2];

	fp = fopen(fn, "r");
	if (fp == NULL)
		goto out;

again:
	cnt = 0;
	for (;;) {
		c[0] = 0;
		switch (fscanf(fp, "%u%1[,-]", &num, c)) {
		case 2:
			if (c[0] == '-') {
				range = 1;
				start = num;
				continue;
			}
			/* fall-through */
		case 1:
			if (range) {
				range = 0;
				cnt += num - start + 1;
			} else {
				start = num;
				cnt++;
			}

			if (ci) {
				uint32_t	i;

				for (i = start; i <= num; i++)
					(ci++)->cpu_id = i;
			}

			if (c[0] == ',')
				continue;
		default:
			break;
		}

		break;
	}

	if (ci == NULL && cip != NULL && cnt > 0) {
		ci = dt_calloc(dtp, cnt, sizeof(cpuinfo_t));
		*cip = ci;
		rewind(fp);
		goto again;
	}

	fclose(fp);
out:
	return cnt;
}

void
dt_conf_init(dtrace_hdl_t *dtp)
{
	size_t		len;
	char		*fn;
	int		i;
	dtrace_conf_t	*conf = &dtp->dt_conf;
	cpuinfo_t	*ci;

	/*
	 * Determine number of possible and online CPUs, and obtain the list of
	 * cpuinfo_t structures for the online CPUs.
	 */
	conf->num_possible_cpus = dt_cpuinfo_read(dtp, POSSIBLE_CPUS, NULL);
	conf->num_online_cpus = dt_cpuinfo_read(dtp, ONLINE_CPUS, &conf->cpus);

	if (conf->num_online_cpus == 0 || conf->cpus == NULL)
		return;

	assert(conf->num_possible_cpus >= conf->num_online_cpus);

	conf->max_cpuid = conf->cpus[conf->num_online_cpus - 1].cpu_id;

	/* Retrieve the chip ID (physical_package_id) for each CPU. */
	len = snprintf(NULL, 0, CPU_CHIP_ID, dtp->dt_conf.max_cpuid) + 1;
	fn = dt_alloc(dtp, len);
	if (fn == NULL)
		return;

	for (i = 0, ci = &dtp->dt_conf.cpus[0]; i < conf->num_online_cpus;
	     i++, ci++) {
		FILE	*fp;

		snprintf(fn, len, CPU_CHIP_ID, ci->cpu_id);
		fp = fopen(fn, "r");
		if (fp != NULL) {
			fscanf(fp, "%u", &ci->cpu_chip);
			fclose(fp);
		}
	}

	dt_free(dtp, fn);
}
