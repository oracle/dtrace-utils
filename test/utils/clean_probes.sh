#!/usr/bin/bash

TRACEFS=/sys/kernel/debug/tracing
EVENTS=${TRACEFS}/available_events
KPROBES=${TRACEFS}/kprobe_events
UPROBES=${TRACEFS}/uprobe_events

# Check permissions
if [[ ! -r ${EVENTS} ]]; then
	echo "ERROR: Cannot read ${EVENTS}" > /dev/stderr
	exit 1
elif [[ ! -w ${KPROBES} ]]; then
	echo "ERROR: Cannot write to ${KPROBES}" > /dev/stderr
	exit 1
elif [[ ! -w ${UPROBES} ]]; then
	echo "ERROR: Cannot write to ${UPROBES}" > /dev/stderr
	exit 1
fi

# Scan the list of events for any orphaned DTrace probes
{
	ps -C dtrace -o pid=
	echo '==='
	cat ${EVENTS}
} | \
	gawk -v kfn=${KPROBES} -v ufn=${UPROBES} \
	    'function getTimestamp(dt) {
		 cmd = "date +\"%s.%N\"";
		 cmd | getline dt;
		 close(cmd);
		 return dt;
	     }

	     function timeDiff(t0, t1, s0, n0, s1, n1) {
		 tmp = $0;

		 $0 = t0;
		 sub(/\./, " ");
		 s0 = int($1);
		 n0 = int($2);
		 $0 = t1;
		 sub(/\./, " ");
		 s1 = int($1);
		 n1 = int($2);

		 if (n1 < n0) {
		     s1--;
		     n1 += 1000000000;
		 }

		 $0 = tmp;

		 return sprintf("%d.%09d", s1 - s0, n1 - n0);
	     }

	     BEGIN { start = getTimestamp(); }

	     /^===/ {
		 re = "^dt_(" substr(pids, 2) ")_";
		 next;
	     }

	     !re {
		 pids = pids "|" int($1);
		 next;
	     }

	     $1 !~ /^dt_[0-9]/ { next; }
	     $1 ~ re { next; }
	     { sub(/:/, "/"); }

	     {
		 if (/_fbt_/)
		     kpv[kpc++] = $0;
		 else
		     upv[upc++] = $0;
	     }

	     END {
		 for (i = 0; i < kpc; i++) {
		     print "Orphaned kprobe "kpv[i];
		     print "-:"kpv[i] >> kfn;
		     if (i % 20 == 19)
			 close(kfn);
		 }
		 close(kfn);

		 for (i = 0; i < upc; i++) {
		     print "Orphaned uprobe "upv[i];
		     print "-:"upv[i] >> ufn;
		     if (i % 20 == 19)
			 close(ufn);
		 }
		 close(ufn);

		 diff = timeDiff(start, getTimestamp());
		 txt = sprintf(" (%d kprobes, %d uprobes)", kpc, upc);
		 print "Cleaning took " diff txt;
	     }'

exit 0
