
# printf

Displays and controls the formatting of a string.

```
void printf(string *format*, ...)
```

The `printf` function is a data recording function that traces expressions and enables elaborate `printf`-style formatting. The parameters consist of a *format* string, followed by a variable number of arguments. The arguments are traced to the directed buffer and are later formatted for output by the `dtrace` command, according to the specified format string.

See the `printf(1)` manual page for more information on formatting directives. Note that although DTrace's implementation of `printf` is aligned with the correlating system function, some differences apply. Notably, you can use the `%d` formatting directive to represent any length of an integer.

## How to use printf to print a formatted string

```
BEGIN {
   printf("execname is %s; priority is %d", execname, curlwpsinfo->pr_pri);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

