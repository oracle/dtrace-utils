
# strjoin

Concatenates two specified strings and returns the resulting string.

```
string strjoin(const char **string1*, const char **string2*)
```

The `strjoin` function returns the concatenation of two specified strings. The returned string is allocated out of scratch memory and is therefore valid only during processing of the clause. If insufficient scratch memory is available, `strjoin` doesn't run and an error is generated.

## How to use strjoin to concatenate two strings together

```
BEGIN {
     string1="foo";
     string2="bar";
     printf("%s",strjoin(string1,string2));
     exit(0);
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

