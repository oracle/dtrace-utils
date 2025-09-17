
# basename

Returns a string excluding any prefix ending in `/`.

```
string basename(const char **str*)
```

The `basename` function creates a string that consists of a copy of the specified string, *str*, but excludes any prefix that ends in `/`, such as a directory path. The returned string is allocated out of scratch memory, and is therefore valid only during the processing of the clause. If insufficient scratch memory is available, `basename` doesn't run and an error is generated.

## How to use basename to return the last element of a path in a string

```
BEGIN
{
        printf("%s\n", basename("/foo/bar/baz"));
        printf("%s\n", basename("/foo/bar///baz/"));
        printf("%s\n", basename("/foo/bar/baz/"));
        printf("%s\n", basename("/foo/bar/baz//"));
}
```

Each of these statements renders the output: `baz`.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

