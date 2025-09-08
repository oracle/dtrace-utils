
# Default Action

The default action applies when DTrace encounters an empty clause for a probe. The default action is to trace the enabled probe identifier \(EPID\).

The default action copies trace data from the EPID to the principal buffer. The following information is returned: CPU, probe ID, probe function, and probe name.

The default action provides the most direct use of the `dtrace` command. For example, running the following command enables all the probes in the `vmlinux` module with the default action:

```
sudo dtrace -m vmlinux
```

Output similar to the following is displayed:

```
dtrace: description 'vmlinux' matched 35 probes
CPU     ID                    FUNCTION:NAME
  0     42                 __schedule:sleep 
  0     34             dequeue_task:dequeue 
  0     40               __schedule:off-cpu 
  0     23        finish_task_switch:on-cpu 
  0     24             enqueue_task:enqueue 
  0     41               __schedule:preempt 
...
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

