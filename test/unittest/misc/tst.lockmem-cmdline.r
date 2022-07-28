1

             1234
0

             1234
0
-- @@stderr --
dtrace: could not enable tracing: failed to create BPF map 'state':
	The kernel locked-memory limit is possibly too low.  Set a
	higher limit with the DTrace option '-xlockmem=N'.  Or, use
	'ulimit -l N' (Kbytes).  Or, make N the string 'unlimited'.
