
# max

Stores the largest value among the specified expressions in an aggregation.

```
void max(*expr*)
```

The `max` function is an aggregation function to store the largest value for an expression in an aggregation.

## How to use max to display the maximum time that processes spend in the system write call

The example stores the timestamp for the `syscall::write:entry` probe fires and then subtracts this value from the timestamp when the `syscall::write:return` fires. The maximum time is calculated based on the time difference between the two probes and stored in an aggregation so that it can be updated for each process that runs. When the program exits, the aggregated maximum timestamp value is displayed for each process identified by the built-in variable `execname`.

```
syscall::write:entry
{
  self->ts = timestamp;
}

syscall::write:return
/self->ts/
{
  @time[execname] = max(timestamp - self->ts);
  self->ts = 0;
}
```

Output similar to the following is displayed when the program exits:

```
  ProxyResolution                                                4891
  firewalld                                                      7892
  RDD Process                                                   11028
  Utility Process                                               11344
  gdbus                                                         11474
  GLXVsyncThread                                                14181
  python3                                                       15286
  Socket Process                                                15294
  rtkit-daemon                                                  16547
  pmdakvm                                                       17089
  NetworkManager                                                18246
  pmdaxfs                                                       19661
  sudo                                                          19917
...
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

