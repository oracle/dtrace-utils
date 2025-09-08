
# system

Causes a specified program to be run on the system as if within a shell.

```
void system(const char *command*)
```

The `system` function is a destructive function that causes the specified program to be run as though provided to the shell as input. The program string can contain any of the `printf` or `printa` format conversions. Arguments that match the format conversions must be specified.

Note that a command specified for the `system` function doesn't run in the context of the firing probe. Rather, it occurs when the buffer containing the details of the `system` function are processed at user level.

## How to use system to run the system date command after every second

Note that the pragma lines include the destructive option to permit DTrace to run destructive functions for this example.

```
#pragma D option destructive
#pragma D option quiet

tick-1sec
{
system("date")
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

