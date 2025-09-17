
# stddev

Stores the standard deviation of the specified expressions in an aggregation.

```
void stddev(*expr*)
```

The `stddev` function is an aggregation function that returns the standard deviation for an expression.

The standard deviation is imprecisely approximated as `√((Σ(x2)/N)-(Σx/N)2)`. This value is sufficient for most DTrace purposes.

## How to use stddev to display the standard deviation of time taken to run processes

The example stores the timestamp for the `syscall::execve:entry` probe fires and then subtracts this value from the timestamp when the `syscall::execve:return` fires. The standard deviation is calculated based on the time difference between the two probes and stored in an aggregation so that it can be updated for each process that runs. When the program exits, the aggregated standard deviation value is displayed.

```
syscall::execve:entry
{
 self->ts = timestamp;
}

syscall::execve:return
/ self->ts /
{
  t = timestamp - self->ts;
  @execsd[execname] = stddev(t);
  self->ts = 0;
}

END
{
  printf("\nSTDDEV:");
  printa(@execsd);
}
```

Output similar to the following is displayed when the program exits:

```
STDDEV:
  head                                                              0
  lsb_release                                                       0
  mkdir                                                             0
  pidof                                                             0
  pkla-check-auth                                                   0
  tr                                                                0
  uname                                                             0
  getopt                                                         5646
  basename                                                       7061
  sed                                                            7236

```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

