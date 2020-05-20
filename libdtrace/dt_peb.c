/*
 * Oracle Linux DTrace.
 * Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/perf_event.h>

#include <dt_impl.h>
#include <dt_bpf.h>
#include <dt_peb.h>

/*
 * Find last set bit in a 64-bit value.
 */
static inline uint64_t fls64(uint64_t n)
{
	if (n == 0)
		return 0;

	return 64 - __builtin_clzl(n);
}

/*
 * Round a given value up to the nearest power of 2.
 */
static inline uint64_t
roundup_pow2(uint64_t n)
{
	return 1UL << fls64(n - 1);
}

/*
 * Close a given perf event buffer.
 */
static void
dt_peb_close(dt_peb_t *peb)
{
	dt_pebset_t	*pebs;

	if (peb == NULL || peb->dtp == NULL || peb->fd == -1)
		return;

	ioctl(peb->fd, PERF_EVENT_IOC_DISABLE, 0);

	pebs = peb->dtp->dt_pebset;
	munmap(peb->base, pebs->page_size + pebs->data_size);

	close(peb->fd);

	peb->base = NULL;
	peb->fd = -1;
}

/*
 * Set up a perf event buffer.
 */
static int
dt_peb_open(dt_peb_t *peb)
{
	int			fd;
	struct perf_event_attr	attr;
	dt_pebset_t		*pebs = peb->dtp->dt_pebset;

	/*
	 * Event configuration for BPF-generated output in perf_event ring
	 * buffers.
	 */
	memset(&attr, 0, sizeof(attr));
	attr.config = PERF_COUNT_SW_BPF_OUTPUT;
	attr.type = PERF_TYPE_SOFTWARE;
	attr.sample_type = PERF_SAMPLE_RAW;
	attr.sample_period = 1;
	attr.wakeup_events = 1;
	fd = perf_event_open(&attr, -1, peb->cpu, -1, PERF_FLAG_FD_CLOEXEC);
	if (fd < 0)
		goto fail;

	/*
	 * We add buf->page_size to the buf->data_size, because perf maintains
	 * a meta-data page at the beginning of the memory region.  That page
	 * is used for reader/writer symchronization.
	 */
	peb->fd = fd;
	peb->base = mmap(NULL, pebs->page_size + pebs->data_size,
			 PROT_READ | PROT_WRITE, MAP_SHARED, peb->fd, 0);
	if (peb->base == MAP_FAILED)
		goto fail;
	peb->endp = peb->base + pebs->page_size + pebs->data_size - 1;

	if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) < 0)
		goto fail;

	return fd;

fail:
	if (peb->base != MAP_FAILED)
		munmap(peb->base, pebs->page_size + pebs->data_size);

	peb->base = NULL;
	peb->endp = NULL;

	if (peb->fd != -1) {
		close(peb->fd);
		peb->fd = -1;
	}

	return -1;
}

/*
 * Perform cleanup of the perf event buffers.
 */
void
dt_pebs_exit(dtrace_hdl_t *dtp)
{
	int	i;

	if (dtp->dt_pebset == NULL)
		return;

	for (i = 0; i < dtp->dt_conf.num_online_cpus; i++)
		dt_peb_close(&dtp->dt_pebset->pebs[i]);

	dt_free(dtp, dtp->dt_pebset->pebs);
	dt_free(dtp, dtp->dt_pebset);

	dtp->dt_pebset = NULL;
}

/*
 * Initialize the perf event buffers (one per online CPU).  Each buffer will
 * the given number of pages (i.e. the total size of each buffer will be
 * num_pages * getpagesize()).  The  allocated memory for each buffer is mmap'd
 * so the kernel can write to it, and its representative file descriptor is
 * recorded in the 'buffers' BPF map so that BPF code knows where to write
 * trace data for a specific CPU.
 *
 * An event polling file descriptor is created as well, and it is configured to
 * monitor all perf event buffers at once.  This file descriptor is returned
 * upon success..  Failure is indicated with a -1 return value.
 */
int dt_pebs_init(dtrace_hdl_t *dtp, size_t bufsize)
{
	int		i;
	int		mapfd;
	size_t		num_pages;
	dt_ident_t	*idp;
	dt_peb_t	*pebs;

	/*
	 * The perf event buffer implementation in the kernel requires that the
	 * buffer comprises n full memory pages, where n must be a power of 2.
	 * Since the buffer size is specified in bytes in DTrace, we need to
	 * convert this size (and possibly round it up) to an acceptable value.
	 */
	num_pages = roundup_pow2((bufsize + getpagesize() - 1) / getpagesize());
	if (num_pages * getpagesize() > bufsize)
		fprintf(stderr, "bufsize increased to %lu\n",
			num_pages * getpagesize());

	/*
	 * Determine the fd for the 'buffers' BPF map.
	 */
	idp = dt_dlib_get_map(dtp, "buffers");
	if (idp == NULL || idp->di_id == DT_IDENT_UNDEF)
		return -ENOENT;

	mapfd = idp->di_id;

	/*
	 * Allocate the perf event buffer set.
	 */
	dtp->dt_pebset = dt_zalloc(dtp, sizeof(dt_pebset_t));
	if (dtp->dt_pebset == NULL)
		return -ENOMEM;

	/*
	 * Allocate the per-CPU perf event buffers.
	 */
	pebs = dt_calloc(dtp, dtp->dt_conf.num_online_cpus,
			 sizeof(struct dt_peb));
	if (pebs == NULL) {
		dt_free(dtp, dtp->dt_pebset);
		return -ENOMEM;
	}

	dtp->dt_pebset->pebs = pebs;

	dtp->dt_pebset->page_size = getpagesize();
	dtp->dt_pebset->data_size = num_pages * dtp->dt_pebset->page_size;

	/*
	 * Initialize a perf event buffer for each online CPU.
	 */
	for (i = 0; i < dtp->dt_conf.num_online_cpus; i++) {
		int			cpu = dtp->dt_conf.cpus[i].cpu_id;
		struct epoll_event	ev;
		dt_peb_t		*peb = &dtp->dt_pebset->pebs[i];

		peb->dtp = dtp;
		peb->cpu = cpu;
		peb->fd = -1;

		/*
		 * Try to create the perf event buffer for 'cpu'.
		 */
		if (dt_peb_open(peb) == -1)
			goto fail;

		/*
		 * Add the perf event buffer to the event polling descriptor.
		 * If we cannot add the buffer for polling, we destroy the
		 * buffer and move on.
		 */
		ev.events = EPOLLIN;
		ev.data.ptr = peb;
		assert(dtp->dt_poll_fd >= 0);
		if (epoll_ctl(dtp->dt_poll_fd, EPOLL_CTL_ADD,
			      peb->fd, &ev) == -1) {
			dt_peb_close(peb);
			continue;
		}

		/*
		 * Store the perf event buffer file descriptor in the 'buffers'
		 * BPF map.
		 */
		dt_bpf_map_update(mapfd, &cpu, &peb->fd);
	}

	return 0;

fail:
	dt_pebs_exit(dtp);

	return -1;
}
