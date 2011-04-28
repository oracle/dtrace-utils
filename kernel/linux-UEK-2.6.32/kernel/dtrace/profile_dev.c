/*
 * FILE:	profile_dev.c
 * DESCRIPTION:	Profile Interrupt Tracing: device file handling
 *
 * Copyright (C) 2010 Oracle Corporation
 */

#include <linux/fs.h>
#include <linux/miscdevice.h>

#include "dtrace_dev.h"

static long profile_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	return -EAGAIN;
}

static int profile_open(struct inode *inode, struct file *file)
{
	return -EAGAIN;
}

static int profile_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations profile_fops = {
	.owner  = THIS_MODULE,
        .unlocked_ioctl = profile_ioctl,
        .open   = profile_open,
        .release = profile_close,
};

static struct miscdevice profile_dev = {
	.minor = DT_DEV_PROFILE_MINOR,
	.name = "profile",
	.nodename = "dtrace/provider/profile",
	.fops = &profile_fops,
};

int profile_dev_init(void)
{
	int ret = 0;

	ret = misc_register(&profile_dev);
	if (ret)
		pr_err("%s: Can't register misc device %d\n",
		       profile_dev.name, profile_dev.minor);

	return ret;
}

void profile_dev_exit(void)
{
	misc_deregister(&profile_dev);
}
