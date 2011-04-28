/*
 * FILE:	systrace_dev.c
 * DESCRIPTION:	System Call Tracing: device file handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <trace/syscall.h>
#include <asm/unistd.h>

#include "dtrace.h"
#include "dtrace_dev.h"
#include "systrace.h"

#define SYSTRACE_ARTIFICIAL_FRAMES	1

#define SYSTRACE_SHIFT			16
#define SYSTRACE_ENTRY(id)		((1 << SYSTRACE_SHIFT) | (id))
#define SYSTRACE_RETURN(id)		(id)
#define SYSTRACE_SYSNUM(x)		((int)(x) & ((1 << SYSTRACE_SHIFT) - 1))
#define SYSTRACE_ISENTRY(x)		((int)(x) >> SYSTRACE_SHIFT)

#if ((1 << SYSTRACE_SHIFT) <= NR_syscalls)
# error 1 << SYSTRACE_SHIFT must exceed number of system calls
#endif

void systrace_provide(void *arg, const dtrace_probedesc_t *desc)
{
	int	i;

	if (desc != NULL)
		return;

	for (i = 0; i < NR_syscalls; i++) {
		struct syscall_metadata	*sm = syscall_nr_to_meta(i);

		if (sm == NULL)
			continue;

		if (dtrace_probe_lookup(systrace_id, NULL, sm->name,
					"entry") != 0)
			continue;

		dtrace_probe_create(systrace_id, NULL, sm->name, "entry",
				    SYSTRACE_ARTIFICIAL_FRAMES,
				    (void *)((uintptr_t)SYSTRACE_ENTRY(i)));
		dtrace_probe_create(systrace_id, NULL, sm->name, "return",
				    SYSTRACE_ARTIFICIAL_FRAMES,
				    (void *)((uintptr_t)SYSTRACE_ENTRY(i)));
	}
}

int systrace_enable(void *arg, dtrace_id_t id, void *parg)
{
	return 0;
}

void systrace_disable(void *arg, dtrace_id_t id, void *parg)
{
}

void systrace_destroy(void *arg, dtrace_id_t id, void *parg)
{
}

static long systrace_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	return -EAGAIN;
}

static int systrace_open(struct inode *inode, struct file *file)
{
	return -EAGAIN;
}

static int systrace_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations systrace_fops = {
	.owner  = THIS_MODULE,
        .unlocked_ioctl = systrace_ioctl,
        .open   = systrace_open,
        .release = systrace_close,
};

static struct miscdevice systrace_dev = {
	.minor = DT_DEV_SYSTRACE_MINOR,
	.name = "systrace",
	.nodename = "dtrace/provider/systrace",
	.fops = &systrace_fops,
};

int systrace_dev_init(void)
{
	int	ret = 0;

	ret = misc_register(&systrace_dev);
	if (ret)
		pr_err("%s: Can't register misc device %d\n",
		       systrace_dev.name, systrace_dev.minor);

	return ret;
}

void systrace_dev_exit(void)
{
	misc_deregister(&systrace_dev);
}
