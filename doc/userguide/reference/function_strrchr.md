
# strrchr

Returns a substring that begins at the last matching occurrence of a specified character in a string.

```
string strrchr(const char *, char)
```

The `strrchr` function returns a substring that begins at the last occurrence of a matching character in a specified string. If no match is found, `strrchr` returns `0`. This function doesn't work with wide characters or multibyte characters.

The returned string is allocated out of scratch memory and is therefore valid only during processing of the clause. If insufficient scratch memory is available, `strrchr` doesn't run and an error is generated.

## How to use strrchr to return the pointer to the last occurrence of a character

```
BEGIN
{
        str = "fooeyfooeyfoo";
        c = 'y';
        # the following line prints "yfoo"
        printf("\"%s\"\n", strrchr(str, c));
        exit(0)
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

