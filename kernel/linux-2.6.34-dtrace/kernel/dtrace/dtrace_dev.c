/*
 * FILE:	dtrace_dev.c
 * DESCRIPTION:	Dynamic Tracing: device file handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#include "dtrace.h"
#include "dtrace_dev.h"

uint32_t			dtrace_helptrace_next = 0;
uint32_t			dtrace_helptrace_nlocals;
char				*dtrace_helptrace_buffer;
int				dtrace_helptrace_bufsize = 512 * 1024;

#ifdef DEBUG
int				dtrace_helptrace_enabled = 1;
#else
int				dtrace_helptrace_enabled = 0;
#endif

int				dtrace_opens;

dtrace_pops_t			dtrace_provider_ops = {
	(void (*)(void *, const dtrace_probedesc_t *))dtrace_nullop,
	(void (*)(void *, struct module *))dtrace_nullop,
	(int (*)(void *, dtrace_id_t, void *))dtrace_enable_nullop,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop,
	NULL,
	NULL,
	NULL,
	(void (*)(void *, dtrace_id_t, void *))dtrace_nullop
};

static size_t			dtrace_retain_max = 1024;

static dtrace_id_t		dtrace_probeid_begin;
static dtrace_id_t		dtrace_probeid_end;
static dtrace_id_t		dtrace_probeid_error;

static dtrace_toxrange_t	*dtrace_toxrange;
static int			dtrace_toxranges;
static int			dtrace_toxranges_max;

static dtrace_pattr_t		dtrace_provider_attr = {
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_PRIVATE, DTRACE_STABILITY_PRIVATE, DTRACE_CLASS_UNKNOWN },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
{ DTRACE_STABILITY_STABLE, DTRACE_STABILITY_STABLE, DTRACE_CLASS_COMMON },
};

void dtrace_nullop(void)
{
}

int dtrace_enable_nullop(void)
{
	return 0;
}

static long dtrace_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	return -EAGAIN;
}

static int dtrace_open(struct inode *inode, struct file *file)
{
	dtrace_state_t	*state;
	uint32_t	priv;
	uid_t		uid;

	dtrace_cred2priv(file->f_cred, &priv, &uid);
	if (priv == DTRACE_PRIV_NONE)
		return -EACCES;

	mutex_lock(&dtrace_provider_lock);
	dtrace_probe_provide(NULL, NULL);
	mutex_unlock(&dtrace_provider_lock);

	/* FIXME: mutex_lock(&cpu_lock); */
	mutex_lock(&dtrace_lock);
	dtrace_opens++;
	dtrace_membar_producer();

#ifdef FIXME
	/*
	 * Is this relevant for Linux?  Is there an equivalent?
	 */
	if ((kdi_dtrace_set(KDI_DTSET_DTRACE_ACTIVATE) != 0) {
		dtrace_opens--;
		mutex_unlock(&cpu_lock);
		mutex_unlock(&dtrace_lock);
		return -EBUS);
	}
#endif

	state = dtrace_state_create(file);
	/* FIXME: mutex_unlock(&cpu_lock); */

	if (state == NULL) {
#ifdef FIXME
		if (--dtrace_opens == 0)
			(void)kdi_dtrace_set(KDI_DTSET_DTRACE_DEACTIVATE);
#endif

		mutex_unlock(&dtrace_lock);

		return -EAGAIN;
	}

	mutex_unlock(&dtrace_lock);

	return 0;
}

static int dtrace_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations dtrace_fops = {
	.owner  = THIS_MODULE,
        .unlocked_ioctl = dtrace_ioctl,
        .open   = dtrace_open,
        .release = dtrace_close,
};

static struct miscdevice dtrace_dev = {
	.minor = DT_DEV_DTRACE_MINOR,
	.name = "dtrace",
	.nodename = "dtrace/dtrace",
	.fops = &dtrace_fops,
};

/*
 * Register a toxic range.
 */
static void dtrace_toxrange_add(uintptr_t base, uintptr_t limit)
{
	if (dtrace_toxranges >= dtrace_toxranges_max) {
		int			osize, nsize;
		dtrace_toxrange_t	*range;

		osize = dtrace_toxranges_max * sizeof (dtrace_toxrange_t);

		if (osize == 0) {
			BUG_ON(dtrace_toxrange != NULL);
			BUG_ON(dtrace_toxranges_max != 0);

			dtrace_toxranges_max = 1;
		} else
			dtrace_toxranges_max <<= 1;

		nsize = dtrace_toxranges_max * sizeof (dtrace_toxrange_t);
		range = kzalloc(nsize, GFP_KERNEL);

		if (dtrace_toxrange != NULL) {
			BUG_ON(osize == 0);

			memcpy(range, dtrace_toxrange, osize);
			kfree(dtrace_toxrange);
		}

		dtrace_toxrange = range;
	}

	BUG_ON(dtrace_toxrange[dtrace_toxranges].dtt_base != (uintptr_t)NULL);
	BUG_ON(dtrace_toxrange[dtrace_toxranges].dtt_limit != (uintptr_t)NULL);

	dtrace_toxrange[dtrace_toxranges].dtt_base = base;
	dtrace_toxrange[dtrace_toxranges].dtt_limit = limit;
	dtrace_toxranges++;
}

/*
 * Initialize the DTrace core.
 *
 * Equivalent to: dtrace_attach()
 */
int dtrace_dev_init(void)
{
	dtrace_provider_id_t	id;
	int			rc = 0;

	/* FIXME: mutex_lock(&cpu_lock); */
	mutex_lock(&dtrace_provider_lock);
	mutex_lock(&dtrace_lock);

	/*
	 * Register the device for the DTrace core.
	 */
	rc = misc_register(&dtrace_dev);
	if (rc) {
		pr_err("%s: Can't register misc device %d\n",
		       dtrace_dev.name, dtrace_dev.minor);

		/* FIXME: mutex_unlock(&cpu_lock); */
		mutex_unlock(&dtrace_lock);
		mutex_unlock(&dtrace_provider_lock);

		return rc;
	}

#ifdef FIXME
	dtrace_modload = dtrace_module_loaded;
	dtrace_modunload = dtrace_module_unloaded;
	dtrace_cpu_init = dtrace_cpu_setup_initial;
	dtrace_helpers_cleanup = dtrace_helpers_destroy;
	dtrace_helpers_fork = dtrace_helpers_duplicate;
	dtrace_cpustart_init = dtrace_suspend;
	dtrace_cpustart_fini = dtrace_resume;
	dtrace_debugger_init = dtrace_suspend;
	dtrace_debugger_fini = dtrace_resume;

	register_cpu_setup_func((cpu_setup_func_t *)dtrace_cpu_setup, NULL);

	dtrace_taskq = taskq_create("dtrace_taskq", 1, maxclsyspri, 1, INT_MAX,
				    0);

	dtrace_state_cache = kmem_cache_create(
				"dtrace_state_cache",
				sizeof (dtrace_dstate_percpu_t) * NCPU,
				DTRACE_STATE_ALIGN, NULL, NULL, NULL, NULL,
				NULL, 0);
#endif

	/*
	 * Create the probe hashtables.
	 */
	dtrace_bymod = dtrace_hash_create(
				offsetof(dtrace_probe_t, dtpr_mod),
				offsetof(dtrace_probe_t, dtpr_nextmod),
				offsetof(dtrace_probe_t, dtpr_prevmod));
	dtrace_byfunc = dtrace_hash_create(
				offsetof(dtrace_probe_t, dtpr_func),
				offsetof(dtrace_probe_t, dtpr_nextfunc),
				offsetof(dtrace_probe_t, dtpr_prevfunc));
	dtrace_byname = dtrace_hash_create(
				offsetof(dtrace_probe_t, dtpr_name),
				offsetof(dtrace_probe_t, dtpr_nextname),
				offsetof(dtrace_probe_t, dtpr_prevname));

	/*
	 * Ensure that the X configuration parameter has a legal value.
	 */
	if (dtrace_retain_max < 1) {
		pr_warning("Illegal value (%lu) for dtrace_retain_max; "
			   "setting to 1", (unsigned long)dtrace_retain_max);

		dtrace_retain_max = 1;
	}

	/*
	 * Discover our toxic ranges.
	 */
	dtrace_toxic_ranges(dtrace_toxrange_add);

	/*
	 * Register ourselves as a provider.
	 */
	(void)dtrace_register("dtrace", &dtrace_provider_attr,
			      DTRACE_PRIV_NONE, 0, &dtrace_provider_ops, NULL,
			      &id);

	BUG_ON(dtrace_provider == NULL);
	BUG_ON((dtrace_provider_id_t)dtrace_provider != id);

	/*
	 * Create BEGIN, END, and ERROR probes.
	 */
	dtrace_probeid_begin = dtrace_probe_create(
				(dtrace_provider_id_t)dtrace_provider, NULL,
				NULL, "BEGIN", 0, NULL);
	dtrace_probeid_end = dtrace_probe_create(
				(dtrace_provider_id_t)dtrace_provider, NULL,
				NULL, "END", 0, NULL);
	dtrace_probeid_error = dtrace_probe_create(
				(dtrace_provider_id_t)dtrace_provider, NULL,
				NULL, "ERROR", 1, NULL);

#ifdef FIXME
	dtrace_anon_property();
	mutex_unlock(&cpu_lock);
#endif

	/*
	 * If DTrace helper tracing is enabled, we need to allocate a trace
	 * buffer.
	 */
	if (dtrace_helptrace_enabled) {
		BUG_ON(dtrace_helptrace_buffer != NULL);

		dtrace_helptrace_buffer = kzalloc(dtrace_helptrace_bufsize,
						  GFP_KERNEL);
		dtrace_helptrace_next = 0;
	}

#ifdef FIXME
	/*
	 * There is usually code here to handle the case where there already
	 * are providers when we get to this code.  On Linux, that does not
	 * seem to be possible since the DTrace core module (this code) is
	 * loaded as a dependency for each provider, and thus this
	 * initialization code is executed prior to the initialization code of
	 * the first provider causing the core to be loaded.
	 */
#endif

	mutex_unlock(&dtrace_lock);
	mutex_unlock(&dtrace_provider_lock);

	return 0;
}

void dtrace_dev_exit(void)
{
	misc_deregister(&dtrace_dev);
}
