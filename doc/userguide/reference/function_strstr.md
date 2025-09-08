
# strstr

Returns a substring starting at first occurrence of a specified substring within a string.

```
string strstr(const char **string*, const char **substring*)
```

The `strstr` function returns a substring starting at the first occurrence of a specified substring in the specified string. If the specified string is empty, `strstr` returns an empty string. If no match is found, `strstr` returns `0`.

## How to use strstr to return a substring starting at the first occurrence of a substring in a string

```
 BEGIN {
     string1="foo bar?";
     substring=" ba";
     # the following line prints " bar?"
     printf("%s",strstr(string1,substring));
     exit(0);
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

