
# min

Stores the smallest value among the specified expressions in an aggregation.

```
void min(*expr*)
```

The `min` function is an aggregation function to store the smallest value for an expression in an aggregation.

## How to use max to display the minimum time that processes spend in the system write call

The example stores the timestamp for the `syscall::write:entry` probe fires and then subtracts this value from the timestamp when the `syscall::write:return` fires. The minimum time is calculated based on the time difference between the two probes and stored in an aggregation so that it can be updated for each process that runs. When the program exits, the aggregated minimum timestamp value is displayed for each process identified by the built-in variable `execname`.

```
syscall::write:entry
{
  self->ts = timestamp;
}

syscall::write:return
/self->ts/
{
  @time[execname] = min(timestamp - self->ts);
  self->ts = 0;
}
```

Output similar to the following is displayed when the program exits:

```
  IPC I/O Parent                                                 1087
  gmain                                                          1091
  libvirt-dbus                                                   1501
  pmcd                                                           1601
  libvirtd                                                       1615
  threaded-ml                                                    1673
  Timer                                                          2130
  NetworkManager                                                 2140
  Socket Thread                                                  2275
  InputThread                                                    2420
...
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

