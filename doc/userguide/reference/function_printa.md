
# printa

Displays and controls the formatting of an aggregation

```
void printa([string *format*,] @*aggr* )
```

The `printa` function is a data recording function that enables you to display and format aggregations. The function takes an aggregation and optionally a string to specify the output formatting using [printf](function_printf.md) formatting directives. If no formatting string is specified, `printa` the specified aggregation is displayed using the default format. If *format* is specified, the aggregation is formatted.

See the `printf(1)` manual page for more information on formatting directives. Note that although DTrace's implementation of `printf` is aligned with the correlating system function, some differences apply. Notably, you can use the `%d` formatting directive to represent any length of an integer. Furthermore, `printa` also handles the appropriate formatting for each aggregation.

## How to use printa to print basic formatting for different aggregations

```
BEGIN
{
        @a = avg(1);
        @b = count();
        @c = lquantize(1, 1, 10);
        printa("@a = %@u\n", @a);
        printa("@b = %@u\n", @b);
        printa("@c = %@d\n", @c);
        exit(0);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

