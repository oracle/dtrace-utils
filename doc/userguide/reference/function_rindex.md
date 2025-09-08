
# rindex

Finds the last occurrence of a specific substring within a string.

```
int rindex(const char * *str*, const char * *substr*[, int *start*])
```

The `rindex` function finds the position of the last occurrence of a substring, *substr*, in a string, *str*, starting at an optional position, *start*. If the specified value of start position is less than 0, it's implicitly set to `0`. If the string is an empty string, `rindex` returns `0`. If no match is found for the substring within the string, `rindex` returns `-1`.

## How to use rindex to identify the last occurrence of a substring within a string

```
BEGIN {
         x = "#findthelastpenguininthepenguinstring!";
         y = "penguin";
         printf("The last penguin appears at character %3d\n", rindex(x, y));
         exit(0)
 }
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

