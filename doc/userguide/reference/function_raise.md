
# raise

Sends a specified signal to the running process.

```
void raise(int)
```

The `raise` function is a destructive function that sends the specified signal to the running process. This function is similar to using the `kill` command to send a signal to the process. The `raise` function can be used to send a signal at a precise point in the runtime of the process.

See the `sigaction(2)` and `kill(1)` manual pages for more information on how process signals work.

## How to use raise to stop a running process

The script opens with a pragma to enable destructive functions in DTrace. You can alternatively remove this line and run the script with `dtrace -w`. The predicate for this script evaluates the process id against a provided argument. The clause includes the raise function with a SIGINT signal that stops the process immediately.

```
#pragma D option destructive
syscall::: 
/pid==$1/
{ 
   raise(SIGINT); 
   exit(0) 
}
```

You must provide the process ID that you intend to stop for this script to function correctly. An example test run might be as follows:

```
xclock & sudo dtrace -wn 'syscall::: /pid==$1/{ raise(SIGINT); exit(0) }' $(pidof xclock)
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

