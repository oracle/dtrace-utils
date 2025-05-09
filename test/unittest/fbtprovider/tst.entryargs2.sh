#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2025, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#
# Another test of entry args.
#

dtrace=$1

# Set up test directory.

DIRNAME=$tmpdir/entryargs2.$$.$RANDOM
mkdir -p $DIRNAME
cd $DIRNAME

# Make the trigger.

cat << EOF > main.c
#include <stdio.h>
#include <fcntl.h>  // open()
#include <unistd.h> // lseek(), write(), close()

int main(int c, char **v) {
	int fd = open("tmp.txt", c == 1 ? O_WRONLY : O_RDWR);

	if (fd == -1)
		return 1;

	/* Move the offset, then write to the file. */
	/* (We will overwrite some "middle section" of the file with "========".) */
	lseek(fd, 20, SEEK_SET);
	write(fd, "=========================", 8);
	close(fd);

	return 0;
}
EOF

# Build the trigger.           FIXME do consistent with Sam's changes.

$CC $test_cppflags $test_ldflags main.c
if [ $? -ne 0 ]; then
	echo "failed to link final executable" >&2
	exit 1
fi

# Prepare the D script.

cat << EOF > D.d
/* these definitions come from kernel header include/linux/fs.h */
#define FMODE_READ	(1 << 0)
#define FMODE_WRITE	(1 << 1)
#define FMODE_LSEEK	(1 << 2)
#define FMODE_PREAD	(1 << 3)
#define FMODE_PWRITE	(1 << 4)

#define FMODE_WRITER	(1 << 16)
#define FMODE_CAN_READ	(1 << 17)
#define FMODE_CAN_WRITE	(1 << 18)
#define FMODE_OPENED	(1 << 19)

fbt::vfs_write:entry
/pid == \$target/
{
	mode = ((struct file *)arg0)->f_mode;
	printf("mode READ           : %s\n", mode & FMODE_READ      ? "yes" : "no");
	printf("mode WRITE          : %s\n", mode & FMODE_WRITE     ? "yes" : "no");
	printf("mode LSEEK          : %s\n", mode & FMODE_LSEEK     ? "yes" : "no");
	printf("mode PREAD          : %s\n", mode & FMODE_PREAD     ? "yes" : "no");
	printf("mode PWRITE         : %s\n", mode & FMODE_PWRITE    ? "yes" : "no");
	printf("mode WRITER         : %s\n", mode & FMODE_WRITER    ? "yes" : "no");
	printf("mode CAN_READ       : %s\n", mode & FMODE_CAN_READ  ? "yes" : "no");
	printf("mode CAN_WRITE      : %s\n", mode & FMODE_CAN_WRITE ? "yes" : "no");
	printf("mode OPENED         : %s\n", mode & FMODE_OPENED    ? "yes" : "no");

	printf("buf: %s\n", stringof(copyinstr(arg1)));
	printf("count: %d\n", arg2);
	printf("pos: %d", *((loff_t *)arg3));
	exit(0);
}
EOF

# Run the D script and trigger twice, once with O_WRONLY and then O_RDWR.

for args in "" "dummy"; do

	# Prepare the file to be (over)written.
	rm -f tmp.txt
	echo abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 > tmp.txt

	# Run the D script and trigger.
	$dtrace $dt_flags -c "./a.out $args" -Cqs D.d

        # Report the output file.
	cat tmp.txt
	echo

done

echo success
exit 0
