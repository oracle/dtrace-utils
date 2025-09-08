
# substr

Returns the substring from a string at a specified index position.

```
string substr(const char * *string*, int *index*[, int *length*])
```

The `substr` function returns the substring of a string, *string*, starting at the specified index position, *index*. An optional length parameter, *length*, can be specified to limit the substring to a specified length.

## How to use substr to return a substring from a specified index

In the example, the length of the substring returned is limited to 4 characters.

```
 BEGIN {
     string1="daddyorchips";
     trace(substr(string1,7,4))
     exit(0)
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

