
# sum

Stores the total value of the specified expression in an aggregation.

```
void sum(*expr*)
```

The `sum` function is an aggregation function to used to obtain the total value of a specified expression, *expr*.

## How to use sum to aggregate a value over a period

This example increments a variable, *i*, by 100 every 10 ms until *i* has a value of 1000. An aggregation is used to calculate the sum of values of *i*. This is equal to the expression: 0+100+200+300+400+500+600+700+800+900=4500.

```
BEGIN
{
        i = 0;
}

tick-10ms
/i < 1000/
{
        @a = sum(i);
        i += 100;
}

tick-10ms
/i == 1000/
{
        exit(0);
}
```

**Parent topic:**[DTrace Function Reference](../reference/dtrace_functions.md)

