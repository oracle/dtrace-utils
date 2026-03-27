#!/bin/bash
#
# Oracle Linux DTrace.
# Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
# Licensed under the Universal Permissive License v 1.0 as shown at
# http://oss.oracle.com/licenses/upl.
#

dtrace="$1"
trigger=`pwd`/test/triggers/delaydie
DIRNAME="$tmpdir/libproc-bad-sh_link2.$$.$RANDOM"
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
		  shdri = 0;
		  next;
	      }

	      $1 == shdroff {
		  # Start processing section headers.
		  in_shdrs = 1;
	      }

	      in_shdrs && shdri >= shdrc {
		  # Done with section headers.  Exit and generate corrupt data.
		  in_shdrs = 0;
		  exit;
	      }

	      # Ignore anything other than section headers.
	      !in_shdrs {
		  next;
	      }

	      # Find the first section (shdri > 0) that is not a STRTAB.  Also,
	      # find the offset of the sh_link field in the symtab section
	      # header.
	      # An Elf64_Shdr comprises 7 data lines beyond the current line.
	      {
		  sh_type = $4;
		  if (shdri > 0 && sh_type != "0200" && !bad_sh_link)
		      bad_sh_link = shdri;

		  getline;			# Elf64_Shdr data line 2
		  getline;			# Elf64_Shdr data line 3
		  getline;			# Elf64_Shdr data line 4
		  getline;			# Elf64_Shdr data line 5
		  getline;			# Elf64_Shdr data line 6

		  if (sh_type == "0200")
		      offset = strtonum("0x" $1);

		  getline;			# Elf64_Shdr data line 7
		  getline;			# Elf64_Shdr data line 8

		  shdri++;
	      }

	      END {
		  printf "%08x: %02x00\n", offset, bad_sh_link;
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
	grep "Pbuild_file_symtab: symtab sh_link"

exit $rc
