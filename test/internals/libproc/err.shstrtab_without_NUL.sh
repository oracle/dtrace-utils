#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace="$1"
trigger=`pwd`/test/triggers/delaydie
DIRNAME="$tmpdir/libproc-shstrtab-without-NUL.$$.$RANDOM"
mkdir -p $DIRNAME
cd $DIRNAME

cp $trigger bad-delaydie
xxd -c8 $trigger | \
	gawk '/^00000028:/ {
		  # Get offset of start of section headers.
		  shdroff = substr($3, 3) substr($3, 1, 2) \
			    substr($2, 3) substr($2, 1, 2) ":";
		  next;
	      }

	      /^00000038:/ {
		  # Get section header string table index.
		  shstrtab = strtonum("0x" substr($5, 3) substr($5, 1, 2));
		  shdri = 0;
		  next;
	      }

	      $1 == shdroff {
		  # Start processing section headers.
		  in_shdrs = 1;
	      }

	      in_shdrs && shdri > shstrtab {
		  # Done with section headers.
		  in_shdrs = 0;
	      }

	      # Ignore anything other than section headers.
	      !in_shdrs {
		  next;
	      }

	      # Process a section header.  If it is the shstrtab, record its
	      # offset and size, and trigger the END clause to generate the
	      # corrupt data.
	      {
		  if (shdri++ == shstrtab) {
		      # Offset is in the 4th data line.
		      getline;			# Elf64_Shdr data line 2
		      getline;			# Elf64_Shdr data line 3
		      getline;			# Elf64_Shdr data line 4
		      offset = strtonum("0x" substr($3, 3) substr($3, 1, 2) \
					     substr($2, 3) substr($2, 1, 2));
		      # Size is in the 5th data line.
		      getline;
		      size = strtonum("0x" substr($3, 3) substr($3, 1, 2) \
					   substr($2, 3) substr($2, 1, 2));
		      exit;
		  } else {
		      # Consume the remaining lines for the section header.
		      getline;			# Elf64_Shdr data line 2
		      getline;			# Elf64_Shdr data line 3
		      getline;			# Elf64_Shdr data line 4
		      getline;			# Elf64_Shdr data line 5
		      getline;			# Elf64_Shdr data line 6
		      getline;			# Elf64_Shdr data line 7
		      getline;			# Elf64_Shdr data line 8
		  }
	      }

	      END {
		  while (size > 16) {
		      printf "%08x: 6565 6565 6565 6565 6565 6565 6565 6565\n", offset;
		      offset += 16;
		      size -= 16;
		  }
		  if (size > 0) {
		      printf "%08x:", offset;
		      while (size > 1) {
			  printf " 6565";
			  size -= 2;
		      }
		  }
		  if (size == 1)
			  printf " 65";
	      }' | \
	xxd -r - bad-delaydie

chmod 755 bad-delaydie

# If the problem exists, the dtrace invocation will trigger a SEGV core dump.
# If it is fixed, dtrace will report an error and terminate with exit code 1.

ulimit -c 0
$dtrace $dt_flags -c ./bad-delaydie -qn 'pid$target:a.out::entry { @[probefunc] = count(); }'
rc=$?

if [ $rc -eq 139 ]; then
	echo "Core dumped"
	exit 0
fi

$dtrace $dt_flags -xdebug -c ./bad-delaydie -qn 'pid$target:a.out::entry { @[probefunc] = count(); }' |& \
	grep "\.shstrtab"

exit $rc
