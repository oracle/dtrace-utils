
# dirname

Returns the path up to the last level of a specified string.

```
string dirname(const char **string*)
```

The `dirname` function creates a string that consists of all but the last level of the path name that's specified by a specified string, *string*. The returned string is allocated out of scratch memory and is therefore valid only during processing of the clause. If insufficient scratch memory is available, `dirname` doesn't run and an error is generated.

## How to use dirname to return the path up to the last element in a string

```
BEGIN
{
        printf("%s\n", dirname("/foo/bar/baz"));
        printf("%s\n", dirname("/foo/bar///baz/"));
        printf("%s\n", dirname("/foo/bar/baz/"));
        printf("%s\n", dirname("/foo/bar/baz//"));
}
```

Each of these statements renders the output: `/foo/bar`.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

