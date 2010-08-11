/*
 * FILE:	dtrace_dev.c
 * DESCRIPTION:	Dynamic Tracing: device file handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>

#include "dtrace_dev.h"

static long dtrace_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	return -EAGAIN;
}

static int dtrace_open(struct inode *inode, struct file *file)
{
	return -EAGAIN;
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

int dtrace_dev_init(void)
{
	int ret = 0;

	ret = misc_register(&dtrace_dev);
	if (ret)
		printk(KERN_ERR "%s: Can't register misc device %d\n", dtrace_dev.name, dtrace_dev.minor);

	return ret;
}

void dtrace_dev_exit(void)
{
	misc_deregister(&dtrace_dev);
}
