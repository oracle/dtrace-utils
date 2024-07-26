/*
 * Oracle Linux DTrace.
 * Copyright (c) 2012, 2023, Oracle and/or its affiliates. All rights reserved.
 * Licensed under the Universal Permissive License v 1.0 as shown at
 * http://oss.oracle.com/licenses/upl.
 */

#pragma D depends_on module vmlinux
#pragma D depends_on provider sched

typedef char	*caddr_t;

typedef struct bufinfo {
	int b_flags;			/* buffer status */
	size_t b_bcount;		/* number of bytes */
	caddr_t b_addr;			/* buffer address */
	uint64_t b_lblkno;		/* block # on device */
	uint64_t b_blkno;		/* expanded block # on device */
	size_t b_resid;			/* # of bytes not transferred */
	size_t b_bufsize;		/* size of allocated buffer */
	caddr_t b_iodone;		/* I/O completion routine */
	int b_error;			/* expanded error field */
	dev_t b_edev;			/* extended device */
} bufinfo_t;

inline int B_BUSY = 0x000001;
#pragma D binding "1.0" B_BUSY
inline int B_DONE = 0x000002;
#pragma D binding "1.0" B_DONE
inline int B_ERROR = 0x000004;
#pragma D binding "1.0" B_ERROR
inline int B_PAGEIO = 0x000010;
#pragma D binding "1.0" B_PAGEIO
inline int B_PHYS = 0x000020;
#pragma D binding "1.0" B_PHYS
inline int B_READ = 0x000040;
#pragma D binding "1.0" B_READ
inline int B_WRITE = 0x000100;
#pragma D binding "1.0" B_WRITE
inline int B_ASYNC = 0x000400;
#pragma D binding "1.0" B_ASYNC

#pragma D binding "1.0" translator
translator bufinfo_t < struct buffer_head *B > {
	b_flags = (int)arg1 & 0x01 ? B_WRITE : B_READ;
	b_addr = B->b_data;
	b_bcount = B->b_size;
	b_lblkno = B->b_blocknr;
	b_blkno = B->b_blocknr;
	b_resid = 0;
	b_bufsize = B->b_size;
	b_iodone = (caddr_t)B->b_end_io;
	b_error = 0;			/* b_state ?? */
	b_edev = B->b_bdev->bd_dev;
};

/*
 * From linux/blk_types.h.
 */

/* bit # in bi_flags */
inline int BIO_USER_MAPPED = 6;

/* bit mask in bi_rw */
inline int REQ_WRITE = 0x01;

inline int REQ_SYNC = 0x800;







#pragma D binding "1.6.3" translator
translator bufinfo_t < struct bio *B > {
	b_flags = ((int)B->bi_opf & REQ_WRITE ? B_WRITE : B_READ) |
		((int)B->bi_opf & REQ_SYNC ? 0 : B_ASYNC) |
		((int)B->bi_flags & (1 << BIO_USER_MAPPED) ? B_PAGEIO : B_PHYS);
	b_addr = 0;
	b_bcount = B->bi_iter.bi_size;
	b_lblkno = B->bi_iter.bi_sector;
	b_blkno = B->bi_iter.bi_sector;
	b_resid = 0;
	b_bufsize = B->bi_iter.bi_size;
	b_iodone = (caddr_t)B->bi_end_io;
	b_error = 0;
	b_edev = B->bi_disk == NULL ? 0 : B->bi_disk->part_tbl->part[B->bi_partno]->bd_dev;
};

typedef struct devinfo {
	int dev_major;			/* major number */
	int dev_minor;			/* minor number */
	int dev_instance;		/* instance number */
	string dev_name;		/* name of device */
	string dev_statname;		/* name of device + instance/minor */
	string dev_pathname;		/* pathname of device */
} devinfo_t;



#pragma D binding "1.0" translator
translator devinfo_t < struct buffer_head *B > {
	dev_major = getmajor(B->b_bdev->bd_dev);
	dev_minor = getminor(B->b_bdev->bd_dev);
	dev_instance = 0;		/* not used? */
	dev_name = B->b_bdev->bd_disk->part0->bd_device.parent
	    ? B->b_bdev->bd_disk->part0->bd_device.parent->driver->name
		? stringof(B->b_bdev->bd_disk->part0->bd_device.parent->driver->name)
		: "<none>"
	    : B->b_bdev->bd_disk->part0->bd_device.driver->name
		? stringof(B->b_bdev->bd_disk->part0->bd_device.driver->name)
		: "<none>";
	dev_statname = B->b_bdev->bd_partno == 0
			? stringof(B->b_bdev->bd_disk->disk_name)
			: strjoin(stringof(B->b_bdev->bd_disk->disk_name),
				  lltostr(B->b_bdev->bd_partno));
	dev_pathname = strjoin(
			"/dev/",
			B->b_bdev->bd_partno == 0
			    ? stringof(B->b_bdev->bd_disk->disk_name)
			    : strjoin(stringof(B->b_bdev->bd_disk->disk_name),
				      lltostr(B->b_bdev->bd_partno))
		       );
};

#pragma D binding "1.6.3" translator
translator devinfo_t < struct bio *B > {
	dev_major = B->bi_disk == NULL ? 0 : getmajor(B->bi_disk->part_tbl->part[B->bi_partno]->bd_dev);
	dev_minor = B->bi_disk == NULL ? 0 : getminor(B->bi_disk->part_tbl->part[B->bi_partno]->bd_dev);
	dev_instance = 0;
	dev_name = B->bi_disk == NULL
			? "nfs"
			: stringof(((struct blk_major_name **)&`major_names)[
					getmajor(B->bi_disk->part_tbl->part[B->bi_partno]->bd_dev) % 255
				   ]->name);
	dev_statname = B->bi_disk == NULL ? "nfs" :
	    B->bi_partno == 0 ? stringof(B->bi_disk->disk_name) :
	    strjoin(stringof(B->bi_disk->disk_name), lltostr(B->bi_partno));
	dev_pathname = B->bi_disk == NULL ? "<nfs>" : "<unknown>";
};

typedef struct fileinfo {
	string fi_name;			/* name (basename of fi_pathname) */
	string fi_dirname;		/* directory (dirname of fi_pathname) */
	string fi_pathname;		/* full pathname */
	loff_t fi_offset;		/* offset within file */
	string fi_fs;			/* filesystem */
	string fi_mount;		/* mount point of file system */
	int fi_oflags;			/* open(2) flags for file descriptor */
} fileinfo_t;

#pragma D binding "1.0" translator
translator fileinfo_t < struct buffer_head *B > {
	fi_name = "<unknown>";
	fi_dirname = "<unknown>";
	fi_pathname = "<unknown>";
	fi_offset = 0;
	fi_fs = "<unknown>";
	fi_mount = "<unknown>";
	fi_oflags = 0;
};

inline int O_ACCMODE = 0003;
#pragma D binding "1.1" O_ACCMODE
inline int O_RDONLY = 00;
#pragma D binding "1.1" O_RDONLY
inline int O_WRONLY = 01;
#pragma D binding "1.1" O_WRONLY
inline int O_RDWR = 02;
#pragma D binding "1.1" O_RDWR
inline int O_CREAT = 0100;
#pragma D binding "1.1" O_CREAT
inline int O_EXCL = 0200;
#pragma D binding "1.1" O_EXCL
inline int O_NOCTTY = 0400;
#pragma D binding "1.1" O_NOCTTY
inline int O_TRUNC = 01000;
#pragma D binding "1.1" O_TRUNC
inline int O_APPEND = 02000;
#pragma D binding "1.1" O_APPEND
inline int O_NONBLOCK = 04000;
#pragma D binding "1.1" O_NONBLOCK
inline int O_NDELAY = 04000;
#pragma D binding "1.1" O_NDELAY
inline int O_SYNC = 04010000;
#pragma D binding "1.1" O_SYNC
inline int O_FSYNC = 04010000;
#pragma D binding "1.1" O_FSYNC
inline int O_ASYNC = 020000;
#pragma D binding "1.1" O_ASYNC
inline int O_DIRECTORY = 0200000;
#pragma D binding "1.1" O_DIRECTORY
inline int O_NOFOLLOW = 0400000;
#pragma D binding "1.1" O_NOFOLLOW
inline int O_CLOEXEC = 02000000;
#pragma D binding "1.1" O_CLOEXEC
inline int O_DSYNC = 010000;
#pragma D binding "1.1" O_DSYNC
inline int O_RSYNC = 04010000;
#pragma D binding "1.1" O_RSYNC

#pragma D binding "1.1" translator
translator fileinfo_t < struct file *F > {
	fi_name = F == NULL
			? "<none>"
			: stringof(F->f_path.dentry->d_name.name);
	fi_dirname = F == NULL
			? "<none>"
			: dirname(d_path(&(F->f_path)));
	fi_pathname = F == NULL
			? "<none>"
			: d_path(&(F->f_path));
	fi_offset = F == NULL
			? 0
			: F->f_pos;
	fi_fs = F == NULL
			? "<none>"
			: stringof(F->f_path.mnt->mnt_sb->s_type->name);
	fi_mount = F == NULL
			? "<none>"
			: "<unknown>";
	fi_oflags = F == NULL
			? 0
			: F->f_flags;
};

inline fileinfo_t fds[int fd] = xlate <fileinfo_t> (
				 fd >= 0 && fd < curthread->files->fdt->max_fds
					? curthread->files->fdt->fd[fd]
					: NULL
				);

#pragma D attributes Stable/Stable/Common fds
#pragma D binding "1.1" fds
