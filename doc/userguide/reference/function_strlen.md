
# strlen

Returns the length of a string in bytes.

```
size_t strlen(const char **string*)
```

The `strlen` function returns the length of a specified string in bytes, excluding the terminating null byte.

## How to use strlen to return the length of a string

```
BEGIN {
     string1="foo bar?";
     printf("%d",strlen(string1));
     exit(0);
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

