
# cleanpath

Creates a copy of a path without redundant elements.

```nocopybutton
string cleanpath(char **str*)
```

The `cleanpath` function creates a string consisting of a copy of the path indicated by *str*, but with certain redundant elements eliminated. In particular, `/./` elements in the path are removed, and `/../` elements are collapsed. The collapsing of `/../` elements in the path occurs without regard to symbolic links. Therefore, it's possible that `cleanpath` could take a valid path and return a shorter, invalid path.

For example, if *str* were `/foo/../bar` and `/foo` were a symbolic link to `/net/foo/export`, `cleanpath` would return the string `/bar`, even though `bar` might only exist in `/net/foo` and not in `/`. This limitation is because `cleanpath` is called in the context of a firing probe, where full symbolic link resolution of arbitrary names isn't possible. The returned string is allocated out of scratch memory and is therefore valid only during the clause. If insufficient scratch memory is available, `cleanpath` doesn't execute and an error is generated.

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

