
# count

Stores an incremented count value in an aggregation.

```
void count()
```

The `count` function is an aggregation function that takes no arguments and returns the value for the number of times that it has been called.

## How to use count to display the number of write\(\) system calls by process name

This example uses the `syscall::write:entry` probe and an aggregation to store the count value. The aggregation uses the built-in variable, `execname`, as a key.

```
syscall::write:entry
{
  @counts[execname] = count();
}
```

When run, output similar to the following is displayed when the program exits:

```
dtrace: description 'syscall::write:entry' matched 1 probe
^C
  dirname                                                           1
  dtrace                                                            1
  gnome-panel                                                       1
  ps                                                                1
  basename                                                          2
  gconfd-2                                                          2
  java                                                              2
  bash                                                              9
  cat                                                               9
  gnome-session                                                     9
  Xorg                                                             21
  firefox                                                         149
  gnome-terminal                                                 9421
  ...
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

