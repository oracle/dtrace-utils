
# index

Finds the first occurrence of a substring within a string.

```
int index(const char * *str*, const char * *substr* [, int *start*])
```

The `index` function finds the position of the first occurrence of a substring, *substr*, in a string, *str*, starting at an optional position, *start*. If the specified value of the start position is less than 0, it's implicitly set to `0`. If the string is empty, `index` returns `0`. If no match is found for the substring within the string, `index` returns `-1`.

## How to use index to identify the first occurrence of a substring within a string

```
BEGIN {
         x = "#canyoufindapenguininthisstring?";
         y = "penguin";
         printf("The penguin appears at character %3d\n", index(x, y));
         exit(0)
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

