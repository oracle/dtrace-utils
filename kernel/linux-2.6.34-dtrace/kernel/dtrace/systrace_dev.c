/*
 * FILE:	systrace_dev.c
 * DESCRIPTION:	System Call Tracing: device file handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>

#include "dtrace_dev.h"

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
	int ret = 0;

	ret = misc_register(&systrace_dev);
	if (ret)
		printk(KERN_ERR "%s: Can't register misc device %d\n", systrace_dev.name, systrace_dev.minor);

	return ret;
}

void systrace_dev_exit(void)
{
	misc_deregister(&systrace_dev);
}
