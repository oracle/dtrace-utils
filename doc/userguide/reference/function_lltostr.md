
# lltostr

Converts an unsigned 64-bit integer to a string.

```
string lltostr(int64_t)
```

The `lltostr` function converts an unsigned 64-bit integer to a string. The returned string is allocated out of scratch memory and is therefore valid only during processing of the clause. If insufficient scratch memory is available, `lltostr` doesn't run and an error is generated.

## How to use lltostr to convert a 64-bit integer to a string

The example shows that the `printf` function treats the value as a string. The pragma option in the script sets the maximum string size to 7 bytes, so the string that's returned by the `lltostr` function is truncated to 1234567.

```
#pragma D option strsize=7

BEGIN
{
    printf("%s\n", lltostr(1234567890));
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

