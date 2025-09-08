
# avg

Stores the arithmetic average of the specified expressions in an aggregation.

```
void avg(*expr*)
```

The `avg` function is an aggregation function to return the arithmetic average for a specified D expression.

## How to use avg to display the average time that processes spend in the system write call

The example stores the timestamp for the `syscall::write:entry` probe fires and then subtracts this value from the timestamp when the `syscall::write:return` fires. The average time is calculated based on the time difference between the two probes and stored in an aggregation so that it can be updated for each process that runs. When the program exits, the aggregated average timestamp value is displayed for each process identified by the built-in variable `execname`.

```
syscall::write:entry
{
  self->ts = timestamp;
}

syscall::write:return
/self->ts/
{
  @time[execname] = avg(timestamp - self->ts);
  self->ts = 0;
}
```

Output similar to the following is displayed when the program exits:

```
 gnome-session                                                  8260
  udisks-part-id                                                 9279
  gnome-terminal                                                 9378
  lsof                                                          14903
  ip                                                            15075
  date                                                          15371
  ...
  ps                                                            91792
  sestatus                                                      98374
  pstree                                                       102566
  udisks-daemon                                                250405
  gconfd-2                                                   17880523
  cat                                                        59752284
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

