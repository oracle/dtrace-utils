#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace="$1"
trigger=`pwd`/test/triggers/delaydie
DIRNAME="$tmpdir/libproc-strtab-without-NUL.$$.$RANDOM"
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
		  # Get number of section headers.
                  shdrc = strtonum("0x" substr($3, 3) substr($3, 1, 2));
		  idx = 0;
		  next;
	      }

	      $1 == shdroff {
		  # Start processing section headers.
		  in_shdrs = 1;
	      }

	      in_shdrs && idx >= shdrc {
		  # Done with section headers.  Exit and generate corrupt data.
		  in_shdrs = 0;
		  exit;
	      }

	      # Ignore anything other than section headers.
	      !in_shdrs {
		  next;
	      }

	      # Process a section header, recording its offset and size.
	      # If we encounter the SYMTAB, record its sh_link value.
	      {
		  # Type is in the 1st line.
		  type = $4;

		  # Offset is in the 4th data line.
		  getline;			# Elf64_Shdr data line 2
		  getline;			# Elf64_Shdr data line 3
		  getline;			# Elf64_Shdr data line 4

		  off[idx] = strtonum("0x" substr($3, 3) substr($3, 1, 2) \
					   substr($2, 3) substr($2, 1, 2));

		  # Size is in the 5th data line.
		  getline;			# Elf64_Shdr data line 5

		  siz[idx] = strtonum("0x" substr($3, 3) substr($3, 1, 2) \
					   substr($2, 3) substr($2, 1, 2));

		  # If this is the SYMTAB, get its STRTAB via sh_link.
		  getline;			# Elf64_Shdr data line 6

		  if (type == "0200")
		      strtab = strtonum("0x" substr($3, 3) substr($3, 1, 2) \
					     substr($2, 3) substr($2, 1, 2));

		  # Consume the last line for the section header.
		  getline;			# Elf64_Shdr data line 7
		  getline;			# Elf64_Shdr data line 8

		  idx++;
	      }

	      END {
		  offset = off[strtab];
		  size = siz[strtab];

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
	grep "Pbuild_file_symtab: unterminated strtab"

exit $rc
