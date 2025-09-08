
# strchr

Returns a substring that begins at the first matching occurrence of a specified character in a string.

```
string strchr(const char **string*, char *char*)
```

The `strchr` function returns a substring that matches the first occurrence of a specified character, *char*, in the specified string, *string*. If no match is found, `strstr` returns `0`. Note that this function doesn't work with wide characters or multibyte characters.

The returned string is allocated out of scratch memory and is therefore valid only during processing of the clause. If insufficient scratch memory is available, `strchr` doesn't run and an error is generated.

## How to use strchr to return a string starting at the first occurrence of a character

```
 BEGIN
 {
         str = "fooeyfooeyfoo";
         c = 'y';
         # the following line prints "yfooeyfoo"
         printf("\"%s\"\n", strchr(str, c));
         exit(0)
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

